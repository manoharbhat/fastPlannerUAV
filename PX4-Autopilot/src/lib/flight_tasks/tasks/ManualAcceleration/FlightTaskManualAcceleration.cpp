/****************************************************************************
 *
 *   Copyright (c) 2020 PX4 Development Team. All rights reserved.
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
 * @file FlightTaskManualAcceleration.cpp
 */

#include "FlightTaskManualAcceleration.hpp"

using namespace matrix;

FlightTaskManualAcceleration::FlightTaskManualAcceleration() :
	_stick_acceleration_xy(this)
{};

bool FlightTaskManualAcceleration::activate(const vehicle_local_position_setpoint_s &last_setpoint)
{
	bool ret = FlightTaskManualAltitudeSmoothVel::activate(last_setpoint);

	if (PX4_ISFINITE(last_setpoint.vx)) {
		_velocity_setpoint.xy() = Vector2f(last_setpoint.vx, last_setpoint.vy);

	} else {
		_velocity_setpoint.xy() = Vector2f(_velocity);
	}

	_stick_acceleration_xy.resetPosition();

	if (PX4_ISFINITE(last_setpoint.acceleration[0])) {
		_stick_acceleration_xy.resetAcceleration(Vector2f(last_setpoint.acceleration[0], last_setpoint.acceleration[1]));
	}

	return ret;
}

bool FlightTaskManualAcceleration::update()
{
	bool ret = FlightTaskManualAltitudeSmoothVel::update();

	_stick_yaw.generateYawSetpoint(_yawspeed_setpoint, _yaw_setpoint,
				       _sticks.getPositionExpo()(3) * math::radians(_param_mpc_man_y_max.get()), _yaw, _deltatime);
	_stick_acceleration_xy.generateSetpoints(_sticks.getPositionExpo().slice<2, 1>(0, 0), _yaw, _yaw_setpoint, _position,
			_deltatime);
	_stick_acceleration_xy.getSetpoints(_position_setpoint, _velocity_setpoint, _acceleration_setpoint);

	_constraints.want_takeoff = _checkTakeoff();
	return ret;
}

void FlightTaskManualAcceleration::_ekfResetHandlerPositionXY()
{
	if (PX4_ISFINITE(_position_setpoint(0))) {
		_position_setpoint(0) = _position(0);
		_position_setpoint(1) = _position(1);
	}
}

void FlightTaskManualAcceleration::_ekfResetHandlerVelocityXY()
{
	if (PX4_ISFINITE(_velocity_setpoint(0))) {
		_velocity_setpoint(0) = _velocity(0);
		_velocity_setpoint(1) = _velocity(1);
	}
}

void FlightTaskManualAcceleration::_ekfResetHandlerPositionZ()
{
	if (PX4_ISFINITE(_position_setpoint(2))) {
		_position_setpoint(2) = _position(2);
	}
}

void FlightTaskManualAcceleration::_ekfResetHandlerVelocityZ()
{
	if (PX4_ISFINITE(_velocity_setpoint(2))) {
		_velocity_setpoint(2) = _velocity(2);
	}
}

void FlightTaskManualAcceleration::_ekfResetHandlerHeading(float delta_psi)
{
	if (PX4_ISFINITE(_yaw_setpoint)) {
		_yaw_setpoint += delta_psi;
	}
}
