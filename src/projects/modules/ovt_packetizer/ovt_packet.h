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
#include <memory>
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
// |             			   Session ID    	                   |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |           Payload Length      |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

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
    SI : 0
 	SN : 0
 	TS : Unix timestamp
 	Payload :
 		{
 			"id": 3921931
 			"url": "ovt://host:port/app/stream"
 		}
 <S->C>
 	M  : 0
 	PT : RESPONSE (20)
 	SI : 0
 	SN : 0
 	TS : Unix timestamp
 	Payload :
 		{
 			"id": 3921931
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
						"mediaType" : 0 //0:"video" | 1:"audio" | 2:"data",
						"timebase_num" : 90000,
						"timebase_den" : 90000,
						"bitrate" : 5000000,
						"startFrameTime" : 1293219321,
						"lastFrameTime" : 1932193921,
						"videoTrack" :
						{
							"framerate" : 29.97,
							"width" : 1280,
							"height" : 720
						},
						"audioTrack" :
						{
							"samplerate" : 44100,
							"sampleFormat" : "s16",
							"layout" : "stereo"
						}
					}
				]
			}
		}

 [1] PLAY
 <C->S>
 	M  : 0
 	PT : PLAY (11) | STOP(12)
 	SI : 0
 	SN : 0
 	TS : Unix timestamp
 	Payload :
 		{
 			"id": 3921932
 			"url": "ovt://host:port/app/stream"
 		}

 		<! Later version can be extended to specify tracks or add other options. >

 <S->C>
 	M  : 0
 	PT : RESPONSE (20)
 	SI : 11992
 	SN : 0
 	TS : Unix timestamp
 	Payload :
		{
			"id": 3921932
			"code" : 200 | 404 | 500
			"message" : "ok" | "app/stream not found" | "Internal Server Error",
		}

		while(STOP or DISCONNECTED)
		{
			M  : 0
			PT : MEDIA (21)
			SI : 11992
			SN : 1 ~ rolling
			TS : Unix timestamp
			Payload :
			[Binary - Serialized MediaPacket]
		}

 [2] STOP
 <C->S>
 	M  : 0
 	PT : STOP(12)
 	SI : 11992
 	SN : 0
 	TS : Unix timestamp
 	Payload :
 		{
 			"id": 3921933
 			"url": "ovt://host:port/app/stream"
 		}

 <S->C>
 	M  : 0
 	PT : RESPONSE (20)
 	SI : 11992
 	SN : 0
 	TS : Unix timestamp
 	Payload :
		{
			"id": 3921933
			"code" : 200 | 404 | 500
			"message" : "ok" | "app/stream not found" | "Internal Server Error",
		}

 **********************************************/


#define OVT_VERSION							1
#define OVT_FIXED_HEADER_SIZE				18
#define OVT_DEFAULT_MAX_PACKET_SIZE			1316

#define OVT_PAYLOAD_TYPE_DESCRIBE			11
#define OVT_PAYLOAD_TYPE_PLAY				12
#define OVT_PAYLOAD_TYPE_STOP				13

#define OVT_PAYLOAD_TYPE_ERROR				20

#define OVT_PAYLOAD_TYPE_MEDIA_PACKET		31

// Using MediaPacket (De)Packetizer
#define MEDIA_PACKET_HEADER_SIZE			(32+64+64+64+8+8+32)/8


class OvtPacket
{
public:
	OvtPacket();
	OvtPacket(OvtPacket &src);
	OvtPacket(const ov::Data &data);
	virtual ~OvtPacket();

	bool 		LoadHeader(const ov::Data &data);
	bool 		Load(const ov::Data &data);

	bool		IsHeaderAvailable();
	bool 		IsPacketAvailable();

	uint8_t 	Version();
	bool 		Marker();
	uint8_t 	PayloadType();
	uint16_t 	SequenceNumber();
	uint64_t 	Timestamp();
	uint32_t 	SessionId();
	uint16_t 	PayloadLength();
	uint32_t 	PacketSize();
	const uint8_t*	Payload();

	void 		SetMarker(bool marker_bit);
	void 		SetPayloadType(uint8_t payload_type);
	void 		SetSequenceNumber(uint16_t sequence_number);
	void 		SetTimestampNow();
	void 		SetTimestamp(uint64_t timestamp);
	void 		SetSessionId(uint32_t session_id);

	bool 		SetPayload(const uint8_t *payload, size_t payload_size);

	const uint8_t* GetBuffer();
	const std::shared_ptr<ov::Data>& GetData();

private:
	void 		SetPayloadLength(size_t payload_length);

	bool 		_is_packet_available = false;

	uint8_t		_version;
	uint8_t 	_marker;
	uint8_t 	_payload_type;
	uint16_t 	_sequence_number;
	uint64_t 	_timestamp;
	uint32_t 	_session_id;
	uint16_t 	_payload_length;

	uint8_t *					_buffer;
	std::shared_ptr<ov::Data>	_data;
};
