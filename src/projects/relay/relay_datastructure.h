//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <cstdint>

#include <base/common_types.h>
#include <base/ovlibrary/ovlibrary.h>
#include <base/ovsocket/socket.h>

enum class RelayPacketType : uint8_t
{
	Register,
	CreateStream,
	DeleteStream,
	Packet,
	Error
};

#pragma pack(push, 1)
struct RelayPacketHeader
{
	// TODO(dimiden): The endian between the RelayServer and the RelayClient must match
	// (Currently, it uses little endian)

	// arbitrary number to distinguish different packets
	uint32_t _transaction_id = 0;
	// 1 if this packet indicates start of packet, 0 otherwise
	uint8_t _start_indicator = 0;
	// 1 if this packet indicates end of packet, 0 otherwise
	uint8_t _end_indicator = 0;

	RelayPacketType type = RelayPacketType::Packet;

	// info::application_id_t
	uint32_t _application_id = 0;
	// info::stream_id_t
	uint32_t _stream_id = 0;

	// common::MediaType
	int8_t _media_type = 0;
	uint32_t _track_id = 0U;
	int64_t _pts = 0LL;
	int64_t _dts = 0LL;
	uint64_t _duration = 0ULL;
	// MediaPacketFlag
	uint8_t _flag = 0U;

	uint16_t _data_size = 0;
};

struct RelayPacket : RelayPacketHeader
{

	explicit RelayPacket(RelayPacketType type);
	explicit RelayPacket(const ov::Data *data);

	RelayPacketType GetType() const;

	void SetTransactionId(uint32_t transaction_id);
	uint32_t GetTransactionId() const;

	void SetApplicationId(uint32_t app_id);
	uint32_t GetApplicationId() const;

	void SetStreamId(uint32_t stream_id);
	uint32_t GetStreamId() const;

	void SetMediaType(int8_t media_type);
	int8_t GetMediaType() const;

	void SetTrackId(uint32_t track_id);
	uint32_t GetTrackId() const;

	void SetPts(uint64_t pts);
	uint64_t GetPts() const;

	void SetDts(int64_t dts);
	int64_t GetDts() const;

	void SetDuration(uint64_t duration);
	uint64_t GetDuration() const;

	void SetFlag(uint8_t flag);
	uint8_t GetFlag() const;

	void SetData(const void *data, uint16_t length);
	const void *GetData() const;
	void *GetData();

	void SetDataSize(uint16_t data_size);
	const uint16_t GetDataSize() const;

	void SetStart(bool start);
	void SetEnd(bool end);

	bool IsEnd() const;
	bool IsStart() const;

protected:
	uint8_t _data[ov::MaxSrtPacketSize - sizeof(RelayPacketHeader)];
};

constexpr size_t RelayPacketDataSize = ov::MaxSrtPacketSize - sizeof(RelayPacketHeader);

static_assert(sizeof(RelayPacket) <= ov::MaxSrtPacketSize, "sizeof(RelayPacket) must be less or equal to maximum srt packet size");
#pragma pack(pop)
