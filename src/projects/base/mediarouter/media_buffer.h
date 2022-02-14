//==============================================================================
//
//  Media Buffer
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include <base/common_types.h>

#include <cstdint>
#include <map>

#include "media_type.h"


enum class MediaPacketFlag : uint8_t
{
	Unknown,  // Unknown
	NoFlag,	  // Raw Data (may PFrame, BFrame...)
	Key		  // Key Frame
};

class MediaPacket
{
public:
	// Provider must inform the bitstream format so that MediaRouter can handle it.
	// This constructor is usually used by the Provider to send media packets to the MediaRouter.
	MediaPacket(uint32_t msid, cmn::MediaType media_type, int32_t track_id, const std::shared_ptr<ov::Data> &data, int64_t pts, int64_t dts, cmn::BitstreamFormat bitstream_format, cmn::PacketType packet_type)
		: MediaPacket(msid, media_type, track_id, data, pts, dts, -1LL, MediaPacketFlag::Unknown)
	{
		_bitstream_format = bitstream_format;
		_packet_type = packet_type;
	}

	// This constructor is usually used by the MediaRouter to send media packets to the publishers.
	MediaPacket(uint32_t msid, cmn::MediaType media_type, int32_t track_id, const std::shared_ptr<ov::Data> &data, int64_t pts, int64_t dts, int64_t duration, MediaPacketFlag flag, cmn::BitstreamFormat bitstream_format, cmn::PacketType packet_type)
		: _msid(msid),
		  _media_type(media_type),
		  _track_id(track_id),
		  _data(data),
		  _pts(pts),
		  _dts(dts),
		  _duration(duration),
		  _flag(flag),
		  _bitstream_format(bitstream_format),
		  _packet_type(packet_type)

	{
	}

	// This constructor is usually used by the MediaRouter to send media packets to the publishers.
	MediaPacket(uint32_t msid, cmn::MediaType media_type, int32_t track_id, const std::shared_ptr<ov::Data> &data, int64_t pts, int64_t dts, int64_t duration, MediaPacketFlag flag)
		: _msid(msid),
		  _media_type(media_type),
		  _track_id(track_id),
		  _data(data),
		  _pts(pts),
		  _dts(dts),
		  _duration(duration),
		  _flag(flag)
	{
	}

	MediaPacket(uint32_t msid, cmn::MediaType media_type, int32_t track_id, const ov::Data *data, int64_t pts, int64_t dts, int64_t duration, MediaPacketFlag flag)
		: _msid(msid),
		  _media_type(media_type),
		  _track_id(track_id),
		  _pts(pts),
		  _dts(dts),
		  _duration(duration),
		  _flag(flag)
	{
		if (data != nullptr)
		{
			_data = data->Clone();
		}
		else
		{
			_data = std::make_shared<ov::Data>();
		}
	}

	MediaPacket(uint32_t msid, cmn::MediaType media_type, int32_t track_id, const void *data, int32_t data_size, int64_t pts, int64_t dts, int64_t duration, MediaPacketFlag flag)
		: MediaPacket(msid, media_type, track_id, nullptr, pts, dts, duration, flag)
	{
		if (data != nullptr)
		{
			_data->Append(data, data_size);
		}
		else
		{
			OV_ASSERT2(data_size == 0);
		}
	}

	cmn::MediaType GetMediaType() const noexcept
	{
		return _media_type;
	}

	void SetData(std::shared_ptr<ov::Data> &data)
	{
		_data = data;
	}

	const std::shared_ptr<const ov::Data> GetData() const noexcept
	{
		return _data;
	}

	std::shared_ptr<ov::Data> &GetData() noexcept
	{
		return _data;
	}

	size_t GetDataLength() noexcept
	{
		return _data->GetLength();
	}

	int64_t GetPts() const noexcept
	{
		return _pts;
	}

	void SetPts(int64_t pts)
	{
		_pts = pts;
	}

	int64_t GetDts() const noexcept
	{
		return _dts;
	}

	void SetDts(int64_t dts)
	{
		_dts = dts;
	}

	int64_t GetDuration() const noexcept
	{
		return _duration;
	}

	void SetDuration(int64_t duration)
	{
		_duration = duration;
	}

	int32_t GetTrackId() const noexcept
	{
		return _track_id;
	}

	void SetTrackId(int32_t track_id)
	{
		_track_id = track_id;
	}

	void SetMsid(uint32_t msid)
	{
		_msid = msid;
	}

	uint32_t GetMsid() const
	{
		return _msid;
	}

	MediaPacketFlag GetFlag() const noexcept
	{
		return _flag;
	}
	void SetFlag(MediaPacketFlag flag)
	{
		_flag = flag;
	}

	cmn::BitstreamFormat GetBitstreamFormat() const noexcept
	{
		return _bitstream_format;
	}

	cmn::PacketType GetPacketType() const noexcept
	{
		return _packet_type;
	}

	void SetBitstreamFormat(cmn::BitstreamFormat format)
	{
		_bitstream_format = format;
	}

	void SetPacketType(cmn::PacketType type)
	{
		_packet_type = type;
	}

	void SetFragHeader(const FragmentationHeader *header)
	{
		_frag_hdr = *header;
	}

	FragmentationHeader *GetFragHeader()
	{
		return &_frag_hdr;
	}

	const FragmentationHeader *GetFragHeader() const
	{
		return &_frag_hdr;
	}

	std::shared_ptr<MediaPacket> ClonePacket()
	{
		auto packet = std::make_shared<MediaPacket>(
			GetMsid(),
			GetMediaType(),
			GetTrackId(),
			GetData(),
			GetPts(),
			GetDts(),
			GetDuration(),
			GetFlag(),
			GetBitstreamFormat(),
			GetPacketType());

		packet->_frag_hdr = _frag_hdr;

		return packet;
	}

protected:
	uint32_t _msid = 0;
	cmn::MediaType _media_type = cmn::MediaType::Unknown;
	int32_t _track_id = -1;

	std::shared_ptr<ov::Data> _data = nullptr;

	int64_t _pts = -1LL;
	int64_t _dts = -1LL;
	int64_t _duration = -1LL;
	MediaPacketFlag _flag = MediaPacketFlag::Unknown;
	cmn::BitstreamFormat _bitstream_format = cmn::BitstreamFormat::Unknown;
	cmn::PacketType _packet_type = cmn::PacketType::Unknown;
	FragmentationHeader _frag_hdr;
};
