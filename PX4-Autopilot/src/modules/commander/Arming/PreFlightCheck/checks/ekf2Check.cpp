/****************************************************************************
 *
 *   Copyright (c) 2019 PX4 Development Team. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name PX4 nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

#include "../PreFlightCheck.hpp"

#include <HealthFlags.h>
#include <math.h>
#include <lib/parameters/param.h>
#include <systemlib/mavlink_log.h>
#include <uORB/Subscription.hpp>
#include <uORB/topics/estimator_selector_status.h>
#include <uORB/topics/estimator_states.h>
#include <uORB/topics/estimator_status.h>

bool PreFlightCheck::ekf2Check(orb_advert_t *mavlink_log_pub, vehicle_status_s &vehicle_status, const bool optional,
			       const bool report_fail, const bool enforce_gps_required)
{
	bool success = true; // start with a pass and change to a fail if any test fails
	bool ahrs_present = true;

	int32_t mag_strength_check_enabled = 1;
	param_get(param_find("COM_ARM_MAG_STR"), &mag_strength_check_enabled);

	float hgt_test_ratio_limit = 1.f;
	param_get(param_find("COM_ARM_EKF_HGT"), &hgt_test_ratio_limit);

	float vel_test_ratio_limit = 1.f;
	param_get(param_find("COM_ARM_EKF_VEL"), &vel_test_ratio_limit);

	float pos_test_ratio_limit = 1.f;
	param_get(param_find("COM_ARM_EKF_POS"), &pos_test_ratio_limit);

	float mag_test_ratio_limit = 1.f;
	param_get(param_find("COM_ARM_EKF_YAW"), &mag_test_ratio_limit);

	bool gps_success = true;
	bool gps_present = true;

	// Get estimator status data if available and exit with a fail recorded if not
	uORB::SubscriptionData<estimator_selector_status_s> estimator_selector_status_sub{ORB_ID(estimator_selector_status)};
	uORB::SubscriptionData<estimator_status_s> status_sub{ORB_ID(estimator_status), estimator_selector_status_sub.get().primary_instance};
	status_sub.update();
	const estimator_status_s &status = status_sub.get();

	if (status.timestamp == 0) {
		ahrs_present = false;
		goto out;
	}

	// Check if preflight check performed by estimator has failed
	if (status.pre_flt_fail_innov_heading ||
	    status.pre_flt_fail_innov_vel_horiz ||
	    status.pre_flt_fail_innov_vel_vert ||
	    status.pre_flt_fail_innov_height) {
		if (report_fail) {
			if (status.pre_flt_fail_innov_heading) {
				mavlink_log_critical(mavlink_log_pub, "Preflight Fail: heading estimate not stable");

			} else if (status.pre_flt_fail_innov_vel_horiz) {
				mavlink_log_critical(mavlink_log_pub, "Preflight Fail: horizontal velocity estimate not stable");

			} else if (status.pre_flt_fail_innov_vel_horiz) {
				mavlink_log_critical(mavlink_log_pub, "Preflight Fail: vertical velocity estimate not stable");

			} else if (status.pre_flt_fail_innov_height) {
				mavlink_log_critical(mavlink_log_pub, "Preflight Fail: height estimate not stable");
			}
		}

		success = false;
		goto out;
	}

	if ((mag_strength_check_enabled == 1) && status.pre_flt_fail_mag_field_disturbed) {
		if (report_fail) {
			mavlink_log_critical(mavlink_log_pub, "Preflight Fail: strong magnetic interference detected");
		}

		success = false;
		goto out;
	}

	// check vertical position innovation test ratio
	if (status.hgt_test_ratio > hgt_test_ratio_limit) {
		if (report_fail) {
			mavlink_log_critical(mavlink_log_pub, "Preflight Fail: Height estimate error");
		}

		success = false;
		goto out;
	}

	// check velocity innovation test ratio
	if (status.vel_test_ratio > vel_test_ratio_limit) {
		if (report_fail) {
			mavlink_log_critical(mavlink_log_pub, "Preflight Fail: Velocity estimate error");
		}

		success = false;
		goto out;
	}

	// check horizontal position innovation test ratio
	if (status.pos_test_ratio > pos_test_ratio_limit) {
		if (report_fail) {
			mavlink_log_critical(mavlink_log_pub, "Preflight Fail: Position estimate error");
		}

		success = false;
		goto out;
	}

	// check magnetometer innovation test ratio
	if (status.mag_test_ratio > mag_test_ratio_limit) {
		if (report_fail) {
			mavlink_log_critical(mavlink_log_pub, "Preflight Fail: Yaw estimate error");
		}

		success = false;
		goto out;
	}

	// If GPS aiding is required, declare fault condition if the required GPS quality checks are failing
	if (enforce_gps_required || report_fail) {
		const bool ekf_gps_fusion = status.control_mode_flags & (1 << estimator_status_s::CS_GPS);
		const bool ekf_gps_check_fail = status.gps_check_fail_flags > 0;

		gps_success = ekf_gps_fusion; // default to success if gps data is fused

		if (ekf_gps_check_fail) {
			if (report_fail) {
				// Only report the first failure to avoid spamming
				const char *message = nullptr;

				if (status.gps_check_fail_flags & (1 << estimator_status_s::GPS_CHECK_FAIL_GPS_FIX)) {
					message = "Preflight%s: GPS fix too low";

				} else if (status.gps_check_fail_flags & (1 << estimator_status_s::GPS_CHECK_FAIL_MIN_SAT_COUNT)) {
					message = "Preflight%s: not enough GPS Satellites";

				} else if (status.gps_check_fail_flags & (1 << estimator_status_s::GPS_CHECK_FAIL_MIN_PDOP)) {
					message = "Preflight%s: GPS PDOP too low";

				} else if (status.gps_check_fail_flags & (1 << estimator_status_s::GPS_CHECK_FAIL_MAX_HORZ_ERR)) {
					message = "Preflight%s: GPS Horizontal Pos Error too high";

				} else if (status.gps_check_fail_flags & (1 << estimator_status_s::GPS_CHECK_FAIL_MAX_VERT_ERR)) {
					message = "Preflight%s: GPS Vertical Pos Error too high";

				} else if (status.gps_check_fail_flags & (1 << estimator_status_s::GPS_CHECK_FAIL_MAX_SPD_ERR)) {
					message = "Preflight%s: GPS Speed Accuracy too low";

				} else if (status.gps_check_fail_flags & (1 << estimator_status_s::GPS_CHECK_FAIL_MAX_HORZ_DRIFT)) {
					message = "Preflight%s: GPS Horizontal Pos Drift too high";

				} else if (status.gps_check_fail_flags & (1 << estimator_status_s::GPS_CHECK_FAIL_MAX_VERT_DRIFT)) {
					message = "Preflight%s: GPS Vertical Pos Drift too high";

				} else if (status.gps_check_fail_flags & (1 << estimator_status_s::GPS_CHECK_FAIL_MAX_HORZ_SPD_ERR)) {
					message = "Preflight%s: GPS Hor Speed Drift too high";

				} else if (status.gps_check_fail_flags & (1 << estimator_status_s::GPS_CHECK_FAIL_MAX_VERT_SPD_ERR)) {
					message = "Preflight%s: GPS Vert Speed Drift too high";

				} else {
					if (!ekf_gps_fusion) {
						// Likely cause unknown
						message = "Preflight%s: Estimator not using GPS";
						gps_present = false;

					} else {
						// if we land here there was a new flag added and the code not updated. Show a generic message.
						message = "Preflight%s: Poor GPS Quality";
					}
				}

				if (message) {
					if (enforce_gps_required) {
						mavlink_log_critical(mavlink_log_pub, message, " Fail");

					} else {
						mavlink_log_warning(mavlink_log_pub, message, "");
					}
				}
			}

			gps_success = false;

			if (enforce_gps_required) {
				success = false;
				goto out;
			}
		}
	}

out:
	//PX4_INFO("AHRS CHECK: %s", (success && ahrs_present) ? "OK" : "FAIL");
	set_health_flags(subsystem_info_s::SUBSYSTEM_TYPE_AHRS, ahrs_present, true, success && ahrs_present, vehicle_status);
	set_health_flags(subsystem_info_s::SUBSYSTEM_TYPE_GPS, gps_present, enforce_gps_required, gps_success, vehicle_status);

	return success;
}

bool PreFlightCheck::ekf2CheckStates(orb_advert_t *mavlink_log_pub, const bool report_fail)
{
	float ekf_ab_test_limit = 1.0f; // pass limit re-used for each test
	param_get(param_find("COM_ARM_EKF_AB"), &ekf_ab_test_limit);

	float ekf_gb_test_limit = 1.0f; // pass limit re-used for each test
	param_get(param_find("COM_ARM_EKF_GB"), &ekf_gb_test_limit);

	// Get estimator states data if available and exit with a fail recorded if not
	uORB::SubscriptionData<estimator_selector_status_s> estimator_selector_status_sub{ORB_ID(estimator_selector_status)};
	uORB::Subscription states_sub{ORB_ID(estimator_states), estimator_selector_status_sub.get().primary_instance};

	estimator_states_s states;

	if (states_sub.copy(&states)) {

		// check accelerometer delta velocity bias estimates
		for (uint8_t index = 13; index < 16; index++) {
			// allow for higher uncertainty in estimates for axes that are less observable to prevent false positives
			// adjust test threshold by 3-sigma
			float test_uncertainty = 3.0f * sqrtf(fmaxf(states.covariances[index], 0.0f));

			if (fabsf(states.states[index]) > ekf_ab_test_limit + test_uncertainty) {

				if (report_fail) {
					PX4_ERR("state %d: |%.8f| > %.8f + %.8f", index, (double)states.states[index], (double)ekf_ab_test_limit,
						(double)test_uncertainty);
					mavlink_log_critical(mavlink_log_pub, "Preflight Fail: High Accelerometer Bias");
				}

				return false;
			}
		}

		// check gyro delta angle bias estimates


		if (fabsf(states.states[10]) > ekf_gb_test_limit
		    || fabsf(states.states[11]) > ekf_gb_test_limit
		    || fabsf(states.states[12]) > ekf_gb_test_limit) {

			if (report_fail) {
				mavlink_log_critical(mavlink_log_pub, "Preflight Fail: High Gyro Bias");
			}

			return false;
		}
	}

	return true;
}
