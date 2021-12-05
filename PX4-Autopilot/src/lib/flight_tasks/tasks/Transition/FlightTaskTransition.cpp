/****************************************************************************
 *
 *   Copyright (c) 2018 PX4 Development Team. All rights reserved.
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

/**
 * @file FlightTaskTranstion.cpp
 */

#include "FlightTaskTransition.hpp"

bool FlightTaskTransition::updateInitialize()
{
	return FlightTask::updateInitialize();
}

bool FlightTaskTransition::activate(const vehicle_local_position_setpoint_s &last_setpoint)
{
	_transition_altitude = PX4_ISFINITE(last_setpoint.z) ? last_setpoint.z : _position(2);
	_transition_yaw = PX4_ISFINITE(last_setpoint.yaw) ? last_setpoint.yaw : _yaw;
	return FlightTask::activate(last_setpoint);
}

bool FlightTaskTransition::update()
{
	bool ret = FlightTask::update();
	_acceleration_setpoint.xy() = matrix::Vector2f(0.f, 0.f);
	// demand zero vertical velocity and level attitude
	// tailsitters will override attitude and thrust setpoint
	// tiltrotors and standard vtol will overrride roll and pitch setpoint but keep vertical thrust setpoint
	_position_setpoint.setAll(NAN);
	_velocity_setpoint(2) = 0.0f;

	_yaw_setpoint = NAN;
	return ret;
}
