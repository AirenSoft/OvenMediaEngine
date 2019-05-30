//==============================================================================
//
//  MediaRouteApplication
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

// 인코딩 패킷, 비디오/오디오 프레임을 저장하는 공통 버퍼

#pragma once

#include <cstdint>
#include <vector>
#include <map>

#include "media_type.h"
#include "base/common_types.h"

enum class MediaPacketFlag : uint8_t
{
	NoFlag,
	Key
};

class MediaPacket
{
public:
	MediaPacket(common::MediaType media_type, int32_t track_id, const void *data, int32_t data_size, int64_t pts, MediaPacketFlag flags, int64_t cts = 0)
		: _media_type(media_type),
		  _track_id(track_id),
		  _pts(pts),
		  _flags(flags),
		  _cts(cts)
	{
		_data->Append(data, data_size);
	}

	MediaPacket(common::MediaType media_type, int32_t track_id, const std::shared_ptr<ov::Data> &data, int64_t pts, MediaPacketFlag flags, int64_t cts = 0)
		: _media_type(media_type),
		  _track_id(track_id),
		  _pts(pts),
		  _flags(flags),
		  _cts(cts)
	{
		_data->Append(data.get());
	}

	common::MediaType GetMediaType() const noexcept
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

	int64_t GetPts() const noexcept
	{
		return _pts;
	}

	int32_t GetTrackId() const noexcept
	{
		return _track_id;
	}

	void SetTrackId(int32_t track_id)
	{
		_track_id = track_id;
	}

	MediaPacketFlag GetFlags() const noexcept
	{
		return _flags;
	}

    int64_t GetCts() const noexcept
    {
        return _cts;
    }

    void SetCts(int64_t cts)
    {
        _cts = cts;
    }

	std::unique_ptr<FragmentationHeader> _frag_hdr = std::make_unique<FragmentationHeader>();

	std::unique_ptr<MediaPacket> ClonePacket()
	{
		auto packet = std::make_unique<MediaPacket>(
			GetMediaType(),
			GetTrackId(),
			GetData(),
			GetPts(),
			GetFlags(),
            GetCts()
		);
		::memcpy(packet->_frag_hdr.get(), _frag_hdr.get(), sizeof(FragmentationHeader));
		return packet;
	}

protected:
	common::MediaType _media_type = common::MediaType::Unknown;
	int32_t _track_id = -1;

	std::shared_ptr<ov::Data> _data = std::make_shared<ov::Data>();

	int64_t _pts = -1;
	MediaPacketFlag _flags = MediaPacketFlag::NoFlag;
    int64_t _cts = 0; // cts = pts - dts
};

class MediaFrame
{
public:
	MediaFrame() = default;

	MediaFrame(common::MediaType media_type, int32_t track_id, const uint8_t *data, int32_t data_size, int64_t pts, int32_t flags)
		: _media_type(media_type),
		  _track_id(track_id),
          _pts(pts),
		  _flags(flags)
	{
		SetBuffer(data, data_size);
	}

	MediaFrame(common::MediaType media_type, int32_t track_id, const uint8_t *data, int32_t data_size, int64_t pts)
		: MediaFrame(media_type, track_id, data, data_size, pts, 0)
	{
	}

	MediaFrame(const uint8_t *data, int32_t data_size, int64_t pts)
		: MediaFrame(common::MediaType::Unknown, 0, data, data_size, pts, 0)
	{
	}

	~MediaFrame() = default;

	void ClearBuffer(int32_t plane = 0)
	{
		_data_buffer[plane].clear();
	}

	void SetBuffer(const uint8_t *data, int32_t data_size, int32_t plane = 0)
	{
		_data_buffer[plane].clear();
		_data_buffer[plane].insert(_data_buffer[plane].end(), data, data + data_size);
	}

	void AppendBuffer(const uint8_t *data, int32_t data_size, int32_t plane = 0)
	{
		_data_buffer[plane].insert(_data_buffer[plane].end(), data, data + data_size);
	}

	void AppendBuffer(uint8_t byte, int32_t plane = 0)
	{
		_data_buffer[plane].push_back(byte);
	}

	void InsertBuffer(int offset, const uint8_t *data, int32_t data_size, int32_t plane = 0)
	{
		_data_buffer[plane].insert(_data_buffer[plane].begin() + offset, data, data + data_size);
	}

	const uint8_t *GetBuffer(int32_t plane = 0) const
	{
		auto list = GetPlainData(plane);

		if(list != nullptr)
		{
			return list->data();
		}

		return nullptr;
	}

	uint8_t *GetBuffer(int32_t plane = 0)
	{
		return reinterpret_cast<uint8_t *>(_data_buffer[plane].data());
	}

	uint8_t GetByteAt(int32_t offset, int32_t plane = 0) const
	{
		auto list = GetPlainData(plane);

		if(list != nullptr)
		{
			return (*list)[offset];
		}

		return 0;
	}

	size_t GetDataSize(int32_t plane = 0) const
	{
		auto list = GetPlainData(plane);

		if(list != nullptr)
		{
			return list->size();
		}

		return 0;
	}

	size_t GetBufferSize(int32_t plane = 0) const
	{
		auto list = GetPlainData(plane);

		if(list != nullptr)
		{
			return list->size();
		}

		return 0;
	}

	void EraseBuffer(int32_t offset, int32_t length, int32_t plane = 0)
	{
		_data_buffer[plane].erase(_data_buffer[plane].begin() + offset, _data_buffer[plane].begin() + offset + length);
	}

	// 메모리만 미리 할당함
	void Reserve(uint32_t capacity, int32_t plane = 0)
	{
		_data_buffer[plane].reserve(static_cast<unsigned long>(capacity));
	}

	// 메모리 할당 및 데이터 오브젝트 할당
	// Append Buffer의 성능문제로 Resize를 선작업한다음 GetBuffer로 포인터를 얻어와 데이터를 설정함.
	void Resize(uint32_t capacity, int32_t plane = 0)
	{
		_data_buffer[plane].resize(static_cast<unsigned long>(capacity));
	}

	void SetMediaType(common::MediaType media_type)
	{
		_media_type = media_type;
	}

	common::MediaType GetMediaType() const
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

		if(stride == _stride.cend())
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

	template<typename T>
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
		switch(channels)
		{
			case 1:
				_channels = channels;
				_channel_layout = common::AudioChannel::Layout::LayoutMono;
				break;

			case 2:
				_channels = channels;
				_channel_layout = common::AudioChannel::Layout::LayoutStereo;
				break;
		}
	}

	common::AudioChannel::Layout GetChannelLayout() const
	{
		return _channel_layout;
	}

	void SetChannelLayout(common::AudioChannel::Layout channel_layout)
	{
		switch(channel_layout)
		{
			case common::AudioChannel::Layout::LayoutMono:
				_channel_layout = channel_layout;
				_channels = 1;
				break;

			case common::AudioChannel::Layout::LayoutStereo:
				_channel_layout = channel_layout;
				_channels = 2;
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

	int32_t GetFlags()
	{
		return _flags;
	}

	// This function should only be called before filtering (_track_id 0, 1)
	std::unique_ptr<MediaFrame> CloneFrame()
	{
		auto frame = std::make_unique<MediaFrame>();

		if(_track_id == (int32_t)common::MediaType::Video)
		{
			frame->SetWidth(_width);
			frame->SetHeight(_height);
			frame->SetFormat(_format);
			frame->SetPts(_pts);

			for(int i = 0; i < 3; ++i)
			{
				frame->SetStride(GetStride(i), i);
				frame->SetBuffer(GetBuffer(i), GetDataSize(i), i);
			}
		}
		else if(_track_id == (int32_t)common::MediaType::Audio)
		{
			frame->SetFormat(_format);
			frame->SetBytesPerSample(_bytes_per_sample);
			frame->SetNbSamples(_nb_samples);
			frame->SetChannels(_channels);
			frame->SetSampleRate(_sample_rate);
			frame->SetChannelLayout(_channel_layout);
			frame->SetPts(_pts);

			for(int i = 0; i < _channels; ++i)
			{
				frame->SetBuffer(GetBuffer(i), GetDataSize(i), i);
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
	const std::vector<uint8_t> *GetPlainData(int32_t plane) const
	{
		auto item = _data_buffer.find(plane);

		if(item == _data_buffer.cend())
		{
			return nullptr;
		}

		return &(item->second);
	}

	// Data plane, Data
	std::map<int32_t, std::vector<uint8_t>> _data_buffer;

	// 미디어 타입
	common::MediaType _media_type = common::MediaType::Unknown;

	// 트랙 아이디
	int32_t _track_id = 0;

	// 공통 시간 정보
	int64_t _pts = 0LL;
	size_t _offset = 0;

	std::map<int32_t, int32_t> _stride;

	// 디코딩된 비디오 프레임 정보
	int32_t _width = 0;
	int32_t _height = 0;
	int32_t _format = 0;

	// 디코딩된 오디오 프레임 정보
	int32_t _bytes_per_sample = 0;
	int32_t _nb_samples = 0;
	int32_t _channels = 0;
	common::AudioChannel::Layout _channel_layout = common::AudioChannel::Layout::LayoutMono;
	int32_t _sample_rate = 0;

	int32_t _flags = 0;    // Key, non-Key
};
