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

enum class RelayPacketType : uint8_t
{
	Register,
	CreateStream,
	DeleteStream,
	Packet,
	Error
};

constexpr const int RelayPacketDataSize = 1200;

#pragma pack(push, 1)
struct RelayPacket
{
	explicit RelayPacket(RelayPacketType type)
	{
		this->type = type;
	}

	explicit RelayPacket(const ov::Data *data)
	{
		if(data->GetLength() == sizeof(RelayPacket))
		{
			::memcpy(this, data->GetData(), sizeof(RelayPacket));

			if(GetDataSize() > RelayPacketDataSize)
			{
				OV_ASSERT(GetDataSize() <= RelayPacketDataSize, "Data size %d exceedes %d", GetDataSize(), RelayPacketDataSize);
				_data_size = ov::HostToBE16(RelayPacketDataSize);
			}
		}
		else
		{
			OV_ASSERT(data->GetLength() == sizeof(RelayPacket), "Invalid data size: %zu, expected: %zu", data->GetLength(), sizeof(RelayPacket));
		}
	}

	RelayPacketType GetType() const
	{
		return type;
	}

	void SetTransactionId(uint32_t transaction_id)
	{
		_transaction_id = ov::HostToBE32(transaction_id);
	}

	uint32_t GetTransactionId() const
	{
		return ov::BE32ToHost(_transaction_id);
	}

	void SetApplicationId(uint32_t app_id)
	{
		_application_id = ov::HostToBE32(app_id);
	}

	uint32_t GetApplicationId() const
	{
		return ov::BE32ToHost(_application_id);
	}

	void SetStreamId(uint32_t stream_id)
	{
		_stream_id = ov::HostToBE32(stream_id);
	}

	uint32_t GetStreamId() const
	{
		return ov::BE32ToHost(_stream_id);
	}

	void SetMediaType(int8_t media_type)
	{
		_media_type = media_type;
	}

	int8_t GetMediaType() const
	{
		return _media_type;
	}

	void SetTrackId(uint32_t track_id)
	{
		_track_id = ov::HostToBE32(track_id);
	}

	uint32_t GetTrackId() const
	{
		return ov::BE32ToHost(_track_id);
	}

	void SetPts(uint64_t pts)
	{
		_pts = ov::HostToBE64(pts);
	}

	uint64_t GetPts() const
	{
		return ov::BE64ToHost(_pts);
	}

	void SetFlags(uint8_t flags)
	{
		_flags = flags;
	}

	uint8_t GetFlags() const
	{
		return _flags;
	}

	void SetFragmentHeader(const FragmentationHeader *header)
	{
		::memcpy(&_frag_header, header, sizeof(_frag_header));
	}

	const FragmentationHeader *GetFragmentHeader() const
	{
		return &_frag_header;
	}

	void SetData(const void *data, uint16_t length)
	{
		if(length <= RelayPacketDataSize)
		{
			::memcpy(_data, data, length);
			_data_size = ov::HostToBE16(length);
		}
		else
		{
			OV_ASSERT2(length <= RelayPacketDataSize);
		}
	}

	const void *GetData() const
	{
		return _data;
	}

	void *GetData()
	{
		return _data;
	}

	void SetDataSize(uint16_t data_size)
	{
		_data_size = ov::HostToBE16(data_size);
	}

	const uint16_t GetDataSize() const
	{
		return ov::BE16ToHost(_data_size);
	}

	void SetStart(bool start)
	{
		_start_indicator = static_cast<uint8_t>(start ? 1 : 0);
	}

	void SetEnd(bool end)
	{
		_end_indicator = static_cast<uint8_t>(end ? 1 : 0);
	}

	bool IsEnd() const
	{
		return (_end_indicator == 1);
	}

protected:
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
	uint32_t _track_id = 0;
	uint64_t _pts = 0;
	// MediaPacketFlag
	uint8_t _flags = 0;

	FragmentationHeader _frag_header;

	uint16_t _data_size = 0;
	uint8_t _data[RelayPacketDataSize] = {};
};

static_assert(sizeof(RelayPacket) < 1316, "sizeof(RelayPacket) must be less than 1316");
#pragma pack(pop)
