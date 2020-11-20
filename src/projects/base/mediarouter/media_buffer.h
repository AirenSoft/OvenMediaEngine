//==============================================================================
//
//  Media Buffer
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include <cstdint>
#include <map>

#include <base/common_types.h>
#include "media_type.h"

enum class MediaPacketFlag : uint8_t
{
	Unknwon, // Unknown
	NoFlag, // Raw Data (may PFrame, BFrame...)
	Key // Key Frame
};

class MediaPacket
{
public:
	// Provider must inform the bitstream format so that MediaRouter can handle it.
	// This constructor is usually used by the Provider to send media packets to the MediaRouter.
	MediaPacket(cmn::MediaType media_type, int32_t track_id, const std::shared_ptr<ov::Data> &data, int64_t pts, int64_t dts, cmn::BitstreamFormat bitstream_format, cmn::PacketType packet_type)
		: MediaPacket(media_type, track_id, data, pts, dts, -1LL, MediaPacketFlag::Unknwon)
	{
		_bitstream_format = bitstream_format;
		_packet_type = packet_type;
	}

	// This constructor is usually used by the MediaRouter to send media packets to the publihsers.
	MediaPacket(cmn::MediaType media_type, int32_t track_id, const std::shared_ptr<ov::Data> &data, int64_t pts, int64_t dts, int64_t duration, MediaPacketFlag flag, cmn::BitstreamFormat bitstream_format, cmn::PacketType packet_type)
		: _media_type(media_type),
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

	// This constructor is usually used by the MediaRouter to send media packets to the publihsers.
	MediaPacket(cmn::MediaType media_type, int32_t track_id, const std::shared_ptr<ov::Data> &data, int64_t pts, int64_t dts, int64_t duration, MediaPacketFlag flag)
		: _media_type(media_type),
		  _track_id(track_id),
		  _data(data),
		  _pts(pts),
		  _dts(dts),
		  _duration(duration),
		  _flag(flag)
	{
	}

	MediaPacket(cmn::MediaType media_type, int32_t track_id, const ov::Data *data, int64_t pts, int64_t dts, int64_t duration, MediaPacketFlag flag)
		: _media_type(media_type),
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

	MediaPacket(cmn::MediaType media_type, int32_t track_id, const void *data, int32_t data_size, int64_t pts, int64_t dts, int64_t duration, MediaPacketFlag flag)
		: MediaPacket(media_type, track_id, nullptr, pts, dts, duration, flag)
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

	const std::shared_ptr<const ov::Data> GetData() const noexcept
	{
		return _data;
	}

	std::shared_ptr<ov::Data> &GetData() noexcept
	{
		return _data;
	}

	size_t GetDataLength()  noexcept
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
	cmn::MediaType _media_type = cmn::MediaType::Unknown;
	int32_t _track_id = -1;

	std::shared_ptr<ov::Data> _data = nullptr;

	int64_t _pts = -1LL;
	int64_t _dts = -1LL;
	int64_t _duration = -1LL;
	MediaPacketFlag _flag = MediaPacketFlag::Unknwon;
	cmn::BitstreamFormat _bitstream_format = cmn::BitstreamFormat::Unknwon;
	cmn::PacketType _packet_type = cmn::PacketType::Unknwon;
	FragmentationHeader _frag_hdr;
};

class MediaFrame
{
public:
	MediaFrame() = default;

	MediaFrame(cmn::MediaType media_type, int32_t track_id, const uint8_t *data, int32_t data_size, int64_t pts, int64_t duration, int32_t flags)
		: _media_type(media_type),
		  _track_id(track_id),
		  _pts(pts),
		  _duration(duration),
		  _flags(flags)
	{
		SetBuffer(data, data_size);
	}

	MediaFrame(cmn::MediaType media_type, int32_t track_id, const uint8_t *data, int32_t data_size, int64_t pts, int64_t duration)
		: MediaFrame(media_type, track_id, data, data_size, pts, duration, 0)
	{
	}

	MediaFrame(const uint8_t *data, int32_t data_size, int64_t pts, int64_t duration)
		: MediaFrame(cmn::MediaType::Unknown, 0, data, data_size, pts, duration, 0)
	{
	}

	~MediaFrame() = default;

	void ClearBuffer(int32_t plane = 0)
	{
		auto plane_data = AllocPlainData(plane);

		if (plane_data != nullptr)
		{
			plane_data->Clear();
		}
	}

	void SetBuffer(const uint8_t *data, int32_t data_size, int32_t plane = 0)
	{
		auto plane_data = AllocPlainData(plane);

		if (plane_data != nullptr && data_size != 0)
		{
			plane_data->Clear();
			plane_data->Append(data, data_size);
		}
	}

	void AppendBuffer(const uint8_t *data, int32_t data_size, int32_t plane = 0)
	{
		auto plane_data = AllocPlainData(plane);

		if (plane_data != nullptr && data_size != 0)
		{
			plane_data->Append(data, data_size);
		}
	}

	const uint8_t *GetBuffer(int32_t plane = 0) const
	{
		auto plane_data = GetPlainData(plane);

		if(plane_data != nullptr)
		{
			return plane_data->GetDataAs<uint8_t>();
		}

		return nullptr;
	}

	uint8_t *GetWritableBuffer(int32_t plane = 0)
	{
		auto plane_data = AllocPlainData(plane);

		if (plane_data != nullptr)
		{
			return plane_data->GetWritableDataAs<uint8_t>();
		}

		return nullptr;
	}

	size_t GetBufferSize(int32_t plane = 0) const
	{
		auto plane_data = GetPlainData(plane);

		if (plane_data != nullptr)
		{
			return plane_data->GetLength();
		}

		return 0;
	}

	// 메모리만 미리 할당함
	void Reserve(uint32_t capacity, int32_t plane = 0)
	{
		auto plane_data = AllocPlainData(plane);

		if (plane_data != nullptr)
		{
			plane_data->Reserve(capacity);
		}
	}

	// 메모리 할당 및 데이터 오브젝트 할당
	// Append Buffer의 성능문제로 Resize를 선작업한다음 GetBuffer로 포인터를 얻어와 데이터를 설정함.
	void Resize(uint32_t capacity, int32_t plane = 0)
	{
		auto plane_data = AllocPlainData(plane);

		if (plane_data != nullptr)
		{
			plane_data->SetLength(capacity);
		}
	}

	void SetMediaType(cmn::MediaType media_type)
	{
		_media_type = media_type;
	}

	cmn::MediaType GetMediaType() const
	{
		return _media_type;
	}

	void SetTrackId(int32_t track_id)
	{
		_track_id = track_id;
	}

	int32_t GetTrackId() const
	{
		return _track_id;
	}

	int64_t GetPts() const
	{
		return _pts;
	}

	void SetPts(int64_t pts)
	{
		_pts = pts;
	}

	int64_t GetDuration() const
	{
		return _duration;
	}

	void SetDuration(int64_t duration)
	{
		_duration = duration;
	}

	void SetOffset(size_t offset)
	{
		_offset = offset;
	}

	size_t GetOffset() const
	{
		return _offset;
	}

	void IncreaseOffset(size_t delta)
	{
		_offset += delta;

		OV_ASSERT2(_offset >= 0L);
	}

	void SetStride(int32_t stride, int32_t plane = 0)
	{
		_stride[plane] = stride;
	}

	int32_t GetStride(int32_t plane = 0) const
	{
		const auto &stride = _stride.find(plane);

		if (stride == _stride.cend())
		{
			return 0;
		}

		return stride->second;
	}

	void SetWidth(int32_t width)
	{
		_width = width;
	}

	int32_t GetWidth() const
	{
		return _width;
	}

	void SetHeight(int32_t height)
	{
		_height = height;
	}

	int32_t GetHeight() const
	{
		return _height;
	}

	void SetFormat(int32_t format)
	{
		_format = format;
	}

	int32_t GetFormat() const
	{
		return _format;
	}

	template <typename T>
	T GetFormat() const
	{
		return static_cast<T>(_format);
	}

	int32_t GetBytesPerSample() const
	{
		return _bytes_per_sample;
	}

	void SetBytesPerSample(int32_t bytes_per_sample)
	{
		_bytes_per_sample = bytes_per_sample;
	}

	int32_t GetNbSamples() const
	{
		return _nb_samples;
	}

	void SetNbSamples(int32_t nb_samples)
	{
		_nb_samples = nb_samples;
	}

	int32_t GetChannels() const
	{
		return _channels;
	}

	void SetChannels(int32_t channels)
	{
		switch (channels)
		{
			case 1:
				_channels = channels;
				_channel_layout = cmn::AudioChannel::Layout::LayoutMono;
				break;

			case 2:
				_channels = channels;
				_channel_layout = cmn::AudioChannel::Layout::LayoutStereo;
				break;
		}
	}

	cmn::AudioChannel::Layout GetChannelLayout() const
	{
		return _channel_layout;
	}

	void SetChannelLayout(cmn::AudioChannel::Layout channel_layout)
	{
		switch (channel_layout)
		{
			case cmn::AudioChannel::Layout::LayoutMono:
				_channel_layout = channel_layout;
				_channels = 1;
				break;

			case cmn::AudioChannel::Layout::LayoutStereo:
				_channel_layout = channel_layout;
				_channels = 2;
				break;
			case cmn::AudioChannel::Layout::LayoutUnknown:
			default:
				_channel_layout = channel_layout;
				_channels = 0;
				break;

		}
	}

	int32_t GetSampleRate() const
	{
		return _sample_rate;
	}

	void SetSampleRate(int32_t sample_rate)
	{
		_sample_rate = sample_rate;
	}

	void SetFlags(int32_t flags)
	{
		_flags = flags;
	}

	int32_t GetFlags() const
	{
		return _flags;
	}

	// This function should only be called before filtering (_track_id 0, 1)
	std::shared_ptr<MediaFrame> CloneFrame()
	{
		auto frame = std::make_shared<MediaFrame>();

		if (_media_type == cmn::MediaType::Video)
		{
			frame->SetWidth(_width);
			frame->SetHeight(_height);
			frame->SetFormat(_format);
			frame->SetPts(_pts);
			frame->SetDuration(_duration);

			for (int i = 0; i < 3; ++i)
			{
				frame->SetStride(GetStride(i), i);
				frame->SetPlainData(GetPlainData(i)->Clone(), i);
			}
		}
		else if (_media_type == cmn::MediaType::Audio)
		{
			frame->SetFormat(_format);
			frame->SetBytesPerSample(_bytes_per_sample);
			frame->SetNbSamples(_nb_samples);
			frame->SetChannels(_channels);
			frame->SetSampleRate(_sample_rate);
			frame->SetChannelLayout(_channel_layout);
			frame->SetPts(_pts);
			frame->SetDuration(_duration);

			for (int i = 0; i < _channels; ++i)
			{
				frame->SetPlainData(GetPlainData(i)->Clone(), i);
			}
		}
		else
		{
			OV_ASSERT2(false);
			return nullptr;
		}
		return frame;
	}

private:
	std::shared_ptr<const ov::Data> GetPlainData(int32_t plane) const
	{
		auto item = _data_buffer.find(plane);

		if (item == _data_buffer.cend())
		{
			return nullptr;
		}

		return item->second;
	}

	void SetPlainData(std::shared_ptr<ov::Data> plane_data, int32_t plane)
	{
		_data_buffer[plane] = std::move(plane_data);
	}

	std::shared_ptr<ov::Data> AllocPlainData(int32_t plane)
	{
		auto item = _data_buffer.find(plane);

		if (item == _data_buffer.end())
		{
			auto data = std::make_shared<ov::Data>();

			_data_buffer[plane] = data;

			return std::move(data);
		}

		return item->second;
	}

	// Data plane, Data
	std::map<int32_t, std::shared_ptr<ov::Data>> _data_buffer;
	cmn::MediaType _media_type = cmn::MediaType::Unknown;
	int32_t _track_id = 0;
	int64_t _pts = 0LL;
	int64_t _duration = 0LL;
	size_t _offset = 0;
	std::map<int32_t, int32_t> _stride;

	int32_t _width = 0;
	int32_t _height = 0;
	int32_t _format = 0;

	int32_t _bytes_per_sample = 0;
	int32_t _nb_samples = 0;
	int32_t _channels = 0;
	cmn::AudioChannel::Layout _channel_layout = cmn::AudioChannel::Layout::LayoutMono;
	int32_t _sample_rate = 0;

	int32_t _flags = 0;  // Key, non-Key
};
