/****************************************************************************
 *
 *   Copyright (C) 2019. All rights reserved.
 *   Author: Rui Zheng <ruizheng@femtomes.com>
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

/** @file Femtomes protocol definitions */

#include "gps_helper.h"
#include "../../definitions.h"

class RTCMParsing;

/* ms, timeout for waiting for a response*/
#define FEMTO_RESPONSE_TIMEOUT		200

#define FEMTO_MSG_MAX_LENGTH		256
/* Femtomes ID for UAV output message */
#define FEMTO_MSG_ID_UAVGPS 		8001

/* Femto uavgps message frame premble 3 bytes*/
#define FEMTO_PREAMBLE1			0xaa
#define FEMTO_PREAMBLE2			0x44
#define FEMTO_PREAMBLE3			0x12

/*** femtomes protocol binary message and payload definitions ***/
#pragma pack(push, 4)

/**
* femto_uav_gps_t struct need to be packed
*/
typedef struct {
	uint64_t 	time_utc_usec;		/** Timestamp (microseconds, UTC), this is the timestamp which comes from the gps module. It might be unavailable right after cold start, indicated by a value of 0*/
	int32_t 	lat;			/** Latitude in 1E-7 degrees*/
	int32_t 	lon;			/** Longitude in 1E-7 degrees*/
	int32_t 	alt;			/** Altitude in 1E-3 meters above MSL, (millimetres)*/
	int32_t 	alt_ellipsoid;		/** Altitude in 1E-3 meters bove Ellipsoid, (millimetres)*/
	float 		s_variance_m_s;		/** GPS speed accuracy estimate, (metres/sec)*/
	float 		c_variance_rad;		/** GPS course accuracy estimate, (radians)*/
	float 		eph;			/** GPS horizontal position accuracy (metres)*/
	float 		epv;			/** GPS vertical position accuracy (metres)*/
	float 		hdop;			/** Horizontal dilution of precision*/
	float 		vdop;			/** Vertical dilution of precision*/
	int32_t 	noise_per_ms;		/** GPS noise per millisecond*/
	int32_t 	jamming_indicator;	/** indicates jamming is occurring*/
	float 		vel_m_s;		/** GPS ground speed, (metres/sec)*/
	float 		vel_n_m_s;		/** GPS North velocity, (metres/sec)*/
	float 		vel_e_m_s;		/** GPS East velocity, (metres/sec)*/
	float 		vel_d_m_s;		/** GPS Down velocity, (metres/sec)*/
	float 		cog_rad;		/** Course over ground (NOT heading, but direction of movement), -PI..PI, (radians)*/
	int32_t 	timestamp_time_relative;/** timestamp + timestamp_time_relative = Time of the UTC timestamp since system start, (microseconds)*/
	float 		heading;		/** heading angle of XYZ body frame rel to NED. Set to NaN if not available and updated (used for dual antenna GPS), (rad, [-PI, PI])*/
	uint8_t 	fix_type;		/** 0-1: no fix, 2: 2D fix, 3: 3D fix, 4: RTCM code differential, 5: Real-Time Kinematic, float, 6: Real-Time Kinematic, fixed, 8: Extrapolated. Some applications will not use the value of this field unless it is at least two, so always correctly fill in the fix.*/
	bool 		vel_ned_valid;		/** True if NED velocity is valid*/
	uint8_t 	satellites_used;	/** Number of satellites used*/
} femto_uav_gps_t;

/**
* femto_msg_header_t is femto data header
*/
typedef struct {
	uint8_t 	preamble[3];	/**< Frame header preamble 0xaa 0x44 0x12 */
	uint8_t 	headerlength;	/**< Frame header length ,from the beginning 0xaa */
	uint16_t 	messageid;     /**< Frame message id ,example the FEMTO_MSG_ID_UAVGPS 8001*/
	uint8_t 	messagetype;	/**< Frame message id type */
	uint8_t 	portaddr;	/**< Frame message port address */
	uint16_t 	messagelength; /**< Frame message data length,from the beginning headerlength+1,end headerlength + messagelength*/
	uint16_t 	sequence;
	uint8_t 	idletime;	/**< Frame message idle module time */
	uint8_t 	timestatus;
	uint16_t 	week;
	uint32_t 	tow;
	uint32_t 	recvstatus;
	uint16_t 	resv;
	uint16_t 	recvswver;
} femto_msg_header_t;

/**
* Analysis Femto uavgps frame header
*/
typedef union {
	femto_msg_header_t 	femto_header;
	uint8_t 	 	data[28];
} msg_header_t;

/**
* receive Femto complete uavgps frame
*/
typedef struct {
	uint8_t 		data[256];		/**< receive Frame message content */
	uint32_t 		crc;			/**< receive Frame message crc 4 bytes */
	msg_header_t 		header;			/**< receive Frame message header */
	uint16_t 		read;			/**< receive Frame message read bytes count */
} femto_msg_t;

#pragma pack(pop)
/*** END OF femtomes protocol binary message and payload definitions ***/

enum class FemtoDecodeState {
	pream_ble1,			/**< Frame header preamble first byte 0xaa */
	pream_ble2,			/**< Frame header preamble second byte 0x44 */
	pream_ble3,			/**< Frame header preamble third byte 0x12 */
	head_length,			/**< Frame header length */
	head_data,			/**< Frame header data */
	data,				/**< Frame data */
	crc1,				/**< Frame crc1 */
	crc2,				/**< Frame crc2 */
	crc3,				/**< Frame crc3 */
	crc4				/**< Frame crc4 */
};

class GPSDriverFemto : public GPSHelper
{
public:
	/**
	 * @param heading_offset heading offset in radians [-pi, pi]. It is substracted from the measurement.
	 */
	GPSDriverFemto(GPSCallbackPtr callback, void *callback_user, struct sensor_gps_s *gps_position,
		       float heading_offset = 0.f);
	virtual ~GPSDriverFemto() = default;

	int receive(unsigned timeout) override;
	int configure(unsigned &baudrate, OutputMode output_mode) override;

private:

	/**
	 * caculate the frame crc value
	 */
	uint32_t crc32Value(uint32_t icrc);
	uint32_t calculateBlockCRC32(uint32_t length, uint8_t *buffer, uint32_t crc);

	/**
	 * when Constructor is work, initialize parameters
	 */
	void decodeInit(void);

	/**
	 * check the message if whether is 8001,memcpy data to _gps_position
	 */
	int handleMessage(int len);

	/**
	 * analysis frame data from buf[] to _femto_msg and check the frame is suceess or not
	 */
	int parseChar(uint8_t b);

	/**
	 * Write a command and wait for a (N)Ack
	 * @return 0 on success, <0 otherwise
	 */
	int writeAckedCommandFemto(const char *command, const char *reply, const unsigned timeout);

	/**
	 * receive data for at least the specified amount of time
	 */
	void receiveWait(unsigned timeout_min);


	struct sensor_gps_s 	*_gps_position {nullptr};
	FemtoDecodeState				_decode_state{FemtoDecodeState::pream_ble1};
	femto_uav_gps_t					_femto_uav_gps;
	femto_msg_t 					_femto_msg;
	float 						_heading_offset;
};
