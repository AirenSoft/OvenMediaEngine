//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <cstdint>

#include <base/common_types.h>
#include <base/ovlibrary/ovlibrary.h>

//  0                   1                   2                   3
//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |V=1|M|Reserved-| Payload Type  |       Sequence Number         |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                           Timestamp                           |
// |                              ...                              |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |          Session ID           |        Payload Length         |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

#define OVT_VERSION							1
#define OVT_FIXED_HEADER_SIZE				14
#define OVT_DEFAULT_MAX_PACKET_SIZE			1316

#define OVT_PAYLOAD_TYPE_DESCRIBE			10
#define OVT_PAYLOAD_TYPE_PLAY				11
#define OVT_PAYLOAD_TYPE_STOP				12
#define OVT_PAYLOAD_TYPE_RESPONSE			20
#define OVT_PAYLOAD_TYPE_MEDIA				21

/***********************************************
 * Protocol Specification
 ***********************************************
 OvenTransport is a protocol for Origin-Edge of OvenMediaEngine.
 This is a state protocol and performs both signaling and data transmission with one port.
 Therefore, the connection must be maintained.
 If the Session is disconnected, OVT determines in the same manner as the STOP command.

 [1] DESCRIBE
 <C->S>
 	M  : 0
	PT : DESCRIBE (10)
    SS : 11992
 	SN : 0
 	TS : Unix timestamp
 	Payload :
 		ovt://host:port/app/stream
 <S->C>
 	M  : 0
 	PT : RESPONSE (20)
 	SS : 11992
 	SN : 0
 	TS : Unix timestamp
 	Payload :
 		"response":
 		{
 			"code" : 200 | 404 | 500
 			"message" : "ok" | "app/stream not found" | "Internal Server Error",
			"stream" :
			{
				"appName" : "app",
				"streamName" : "stream_720p",
				"tracks":
				[
					{
						"id" : 3291291,
						"codecId" : 32198392,
						"mediaType" : "video" | "audio" | "data",
						"timebase" : 90000,
						"bitrate" : 5000000,
						"startFrameTime" : 1293219321,
						"lastFrameTime" : 1932193921,
						"videoTrack" :
						{
							"frameRate" : 29.97,
							"width" : 1280,
							"height" : 720
						},
						"audioTrack" :
						{
							"sampleRate" : 44100,
							"sampleFormat" : "s16",
							"layout" : "stereo"
						}
					}
				]
			}
		}

 [1] PLAY | STOP
 <C->S>
 	M  : 0
 	PT : PLAY (11) | STOP(12)
 	SS : 11992
 	SN : 0
 	TS : Unix timestamp
 	Payload : N/A

 <S->C>
 	M  : 0
 	PT : RESPONSE (20)
 	SS : 11992
 	SN : 0
 	TS : Unix timestamp
 	Payload :
 	"response":
 	{
 		"code" : 200 | 404 | 500
 		"message" : "ok" | "app/stream not found" | "Internal Server Error",
 	}

 	if "play"

 	while(STOP or DISCONNECTED)
 	{
 		M  : 0 | 1		# The last packet of the payload has a marker of 1.
		PT : MEDIA (21)
		SS : 11992
		SN : 1 ~ rolling
		TS : Unix timestamp
		Payload :
		[Binary - Serialized MediaPacket]
 	}
 **********************************************/


class OvtPacket
{
public:
	OvtPacket();


private:
	uint8_t		_version;
	uint8_t 	_marker;
	uint8_t 	_packet_type;
	uint8_t 	_sequence_number;
	uint64_t 	_timestamp;
	uint16_t 	_session_id;
	uint16_t 	_payload_length;
};
