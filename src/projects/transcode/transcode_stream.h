//==============================================================================
//
//  TranscodeStream
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "codec/transcode_decoder.h"
#include "codec/transcode_encoder.h"

#include <cstdint>
#include <memory>
#include <vector>
#include <queue>

#include "base/media_route/media_buffer.h"
#include "base/media_route/media_queue.h"
#include "base/media_route/media_type.h"
#include "base/application/stream_info.h"

#include "transcode_context.h"
#include "transcode_filter.h"

#include "codec/transcode_encoder.h"
#include "codec/transcode_decoder.h"

#include <base/application/application.h>

class TranscodeApplication;

typedef int32_t MediaTrackId;

class TranscodeStream
{
public:
	TranscodeStream(const info::Application *application_info, std::shared_ptr<StreamInfo> orig_stream_info, TranscodeApplication *parent);
	~TranscodeStream();

	void Stop();

	bool Push(std::unique_ptr<MediaPacket> packet);

	static std::map<uint32_t, std::set<ov::String>> _stream_list;

private:

	// 입력 스트림 정보
	std::shared_ptr<StreamInfo> _stream_info_input;

	// 미디어 인코딩된 원본 패킷 버퍼
	MediaQueue<std::unique_ptr<MediaPacket>> _queue;

	// 디코딩된 프레임 버퍼
	MediaQueue<std::unique_ptr<MediaFrame>> _queue_decoded;

	// 필터링된 프레임 버퍼
	MediaQueue<std::unique_ptr<MediaFrame>> _queue_filterd;

	// 96-127 dynamic : RTP Payload Types for standard audio and video encodings
	uint8_t _last_track_video = 0x60;     // 0x60 ~ 0x6F
	uint8_t _last_track_audio = 0x70;     // 0x70 ~ 0x7F

private:
	const info::Application *_application_info;

	// 디코더
	std::map<MediaTrackId, std::unique_ptr<TranscodeDecoder>> _decoders;

	// 인코더
	std::map<MediaTrackId, std::unique_ptr<TranscodeEncoder>> _encoders;

	// 필터
	std::map<MediaTrackId, std::unique_ptr<TranscodeFilter>> _filters;

	// 스트림별 트랙집합
	std::map <ov::String, std::vector <uint8_t >> _stream_tracks;


private:
	volatile bool _kill_flag;

	void DecodeTask();
	std::thread _thread_decode;

	void FilterTask();
	std::thread _thread_filter;

	void EncodeTask();
	std::thread _thread_encode;

	TranscodeApplication *_parent;

	// 디코더 생성
	void CreateDecoder(int32_t track_id);

	// 인코더 생성
	void CreateEncoders(std::shared_ptr<MediaTrack> media_track);
	void CreateEncoder(std::shared_ptr<MediaTrack> media_track, std::shared_ptr<TranscodeContext> transcode_context);

	// 디코딩된 프레임의 포맷이 분석되거나 변경될 경우 호출됨.
	void ChangeOutputFormat(MediaFrame *buffer);

	void CreateFilters(std::shared_ptr<MediaTrack> media_track, MediaFrame *buffer);
	void DoFilters(std::unique_ptr<MediaFrame> frame);

	// 1. 디코딩
	TranscodeResult DecodePacket(int32_t track_id, std::unique_ptr<const MediaPacket> packet);
	// 2. 필터링
	TranscodeResult FilterFrame(int32_t track_id, std::unique_ptr<MediaFrame> frame);
	// 3. 인코딩
	TranscodeResult EncodeFrame(int32_t track_id, std::unique_ptr<const MediaFrame> frame);

	// 출력(변화된) 스트림 정보
	bool AddStreamInfoOutput(ov::String stream_name);
	std::map<ov::String, std::shared_ptr<StreamInfo>> _stream_info_outputs;

	// 트랜스코딩 코덱 변환 정보
	uint8_t AddContext(common::MediaType media_type, std::shared_ptr<TranscodeContext> context);
	std::map<MediaTrackId, std::shared_ptr<TranscodeContext>> _contexts;

	// Create output streams
	void CreateStreams();

	// Delete output streams
	void DeleteStreams();

	// Send frame with output stream's information
	void SendFrame(std::unique_ptr<MediaPacket> packet);

	// 통계 정보
private:
	uint32_t _stats_decoded_frame_count;

	uint8_t _stats_queue_full_count;

	uint64_t _max_queue_size;
};

