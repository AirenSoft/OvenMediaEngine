//==============================================================================
//
//  MediaRouteApplication
//
//  Created by Kwon Keuk Hanb
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

// 인코딩 패킷, 비디오/오디오 프레임을 저장하는 공통 버퍼

#pragma once

#include <stdint.h>
#include <vector>
#include <map>

#include "media_type.h"

using namespace MediaCommonType;

class MediaBuffer 
{
public:
	MediaBuffer() {
		_offset = 0;
	}
	MediaBuffer(MediaType media_type, int32_t track_id, uint8_t* data, int32_t data_size, int64_t pts) 
	{
		SetMediaType(media_type);
		SetTrackId(track_id);
		SetBuffer(data, data_size);
		SetPts(pts);
		SetOffset(0);
	}
	MediaBuffer(MediaType media_type, int32_t track_id, uint8_t* data, int32_t data_size, int64_t pts, int32_t flags) 
	{
		SetMediaType(media_type);
		SetTrackId(track_id);
		SetBuffer(data, data_size);
		SetPts(pts);
		SetOffset(0);
		SetFlags(flags);
	}
	MediaBuffer(uint8_t* data, int32_t data_size, int64_t pts) 
	{
		SetBuffer(data, data_size);
		SetPts(pts);
		SetOffset(0);
	}
	~MediaBuffer() 
	{

	}

public:
	void ClearBuffer(int32_t plane = 0)
	{
		_data_buffer[plane].clear();
	}
	
	void SetBuffer(uint8_t* data, int32_t data_size, int32_t plane = 0) 
	{
		_data_buffer[plane].clear();
		_data_buffer[plane].insert(_data_buffer[plane].end(), data, data+data_size);
	}
	
	void AppendBuffer(uint8_t* data, int32_t data_size, int32_t plane = 0) 
	{
		_data_buffer[plane].insert(_data_buffer[plane].end(), data, data+data_size);
	}
	
	void AppendBuffer(uint8_t byte, int32_t plane = 0)
	{
		_data_buffer[plane].push_back(byte);
	}
	
	void InsertBuffer(int offset, uint8_t* data, int32_t data_size, int32_t plane = 0)
	{
		_data_buffer[plane].insert(_data_buffer[plane].begin()+offset, data, data+data_size);	
	}
	
	uint8_t* GetBuffer(int32_t plane = 0)
	{
		return reinterpret_cast<uint8_t*>(_data_buffer[plane].data());
	}
	
	uint8_t GetByteAt(int32_t offset, int32_t plane = 0)
	{
		return _data_buffer[plane][offset];
	}
	
	int32_t  GetDataSize(int32_t plane = 0) 
	{ 
		return _data_buffer[plane].size(); 
	}
	
	uint32_t GetBufferSize(int32_t plane = 0)
	{
		return _data_buffer[plane].size();
	}
	
	void EraseBuffer(int32_t offset, int32_t length, int32_t plane = 0)
	{
		_data_buffer[plane].erase(_data_buffer[plane].begin()+offset, _data_buffer[plane].begin()+offset+length);
	}

	void SetStride(int32_t stride, int32_t plane = 0)
	{
		_stride[plane] = stride;	
	}

	int32_t GetStride(int32_t plane = 0)
	{
		return _stride[plane];	
	}
	
	void SetFlags(int32_t flags)
	{
		_flags = flags;
	}
	
	int32_t GetFlags()
	{
		return _flags;
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

public:
	// Data plane, Data
	std::map<int32_t, std::vector<uint8_t>> 	_data_buffer;

public:

	void SetMediaType(MediaType media_type) 
	{
		_media_type = media_type;
	}

	MediaType GetMediaType()
	{
		return _media_type;
	}

	void SetTrackId(int32_t track_id) 
	{	
		_track_id = track_id;
	}

	int32_t GetTrackId() 
	{
		return _track_id;
	}

	void SetOffset(int32_t offset)
	{
		_offset = offset;
	}
	int32_t GetOffset()
	{
		return _offset;
	}
	
private:
	// 미디어 타입
	MediaType 					_media_type;

	// 트랙 아이디
	int32_t 					_track_id;

	// 공통 시간 정보
public:
	int64_t 					GetPts() { return _pts; }
	void 						SetPts(int64_t pts) { _pts = pts; }
	int64_t 					_pts;


	int32_t						_offset;


	// 디코딩된 비디오 프레임 정보
public:
	void SetWidth(int32_t width)
	{
		_width = width;
	}
	
	void SetHeight(int32_t height)
	{
		_height = height;
	}
	
	void SetFormat(int32_t format)
	{
		_format = format;
	}

	std::map<int32_t, int32_t>  _stride;
	int32_t 					_width;
	int32_t 					_height;
	int32_t 					_format;


	// 디코딩된 오디오 프레임 정보
public:
	int32_t 					_bytes_per_sample;
	int32_t						_nb_samples;
	int32_t						_channels;			
	int32_t						_channel_layout;
	int32_t 					_sample_rate;
	
public:
	int32_t 					_flags;	// Key, non-Key
};
