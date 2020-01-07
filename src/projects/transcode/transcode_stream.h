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
#include "base/info/stream_info.h"

#include "transcode_context.h"
#include "transcode_filter.h"

#include "codec/transcode_encoder.h"
#include "codec/transcode_decoder.h"

#include <base/info/application.h>

typedef int32_t MediaTrackId;

class TranscodeApplication;

class TranscodeStream
{
public:
	TranscodeStream(const info::Application &application_info, const std::shared_ptr<StreamInfo> &orig_stream_info, TranscodeApplication *parent);
	~TranscodeStream();

	void Stop();

	bool Push(std::shared_ptr<MediaPacket> packet);

	static std::map<uint32_t, std::set<ov::String>> _stream_list;

	// For statistics
	uint32_t _stats_decoded_frame_count;
	uint8_t _stats_queue_full_count;
	uint64_t _max_queue_size;

private:
	// Input stream info
	std::shared_ptr<StreamInfo> _stream_info_input;

	// Buffer for encoded(input) media packets
	MediaQueue<std::shared_ptr<MediaPacket>> _queue_input_packets;

	// Buffer for decoded frames
	MediaQueue<std::shared_ptr<MediaFrame>> _queue_decoded_frames;

	// Buffer for filtered frames
	MediaQueue<std::shared_ptr<MediaFrame>> _queue_filterd_frames;

	// 96-127 dynamic : RTP Payload Types for standard audio and video encodings
	uint8_t _last_track_video = 0x60;     // 0x60 ~ 0x6F
	uint8_t _last_track_audio = 0x70;     // 0x70 ~ 0x7F

	const info::Application _application_info;

	// Decoder
	std::map<MediaTrackId, std::shared_ptr<TranscodeDecoder>> _decoders;

	// Encoder
	std::map<MediaTrackId, std::shared_ptr<TranscodeEncoder>> _encoders;

	// Filter
	std::map<MediaTrackId, std::shared_ptr<TranscodeFilter>> _filters;

	// Track Group
	std::map <ov::String, std::vector <uint8_t >> _stream_tracks;

	volatile bool _kill_flag;

	void LoopTask();
	std::thread _thread_looptask;

	TranscodeApplication *_parent;

	void CreateDecoder(int32_t track_id, std::shared_ptr<TranscodeContext> input_context);

	void CreateEncoders(std::shared_ptr<MediaTrack> media_track);
	void CreateEncoder(std::shared_ptr<MediaTrack> media_track, std::shared_ptr<TranscodeContext> output_context);

	// 디코딩된 프레임의 포맷이 분석되거나 변경될 경우 호출됨.
	void ChangeOutputFormat(MediaFrame *buffer);

	void CreateFilters(std::shared_ptr<MediaTrack> media_track, MediaFrame *buffer);
	void DoFilters(std::shared_ptr<MediaFrame> frame);

	// There are 3 steps to process packet
	// Step 1: Decode (Decode a frame from given packets)
	TranscodeResult DecodePacket(int32_t track_id, std::shared_ptr<const MediaPacket> packet);
	// Step 2: Filter (resample/rescale the decoded frame)
	TranscodeResult FilterFrame(int32_t track_id, std::shared_ptr<MediaFrame> frame);
	// Step 3: Encode (Encode the filtered frame to packets)
	TranscodeResult EncodeFrame(int32_t track_id, std::shared_ptr<const MediaFrame> frame);

	// 출력(변화된) 스트림 정보
	bool AddStreamInfoOutput(ov::String stream_name);
	std::map<ov::String, std::shared_ptr<StreamInfo>> _stream_info_outputs;

	// Transcoding information
	uint8_t AddOutputContext(common::MediaType media_type, std::shared_ptr<TranscodeContext> output_context);

	std::map<MediaTrackId, std::shared_ptr<TranscodeContext>> _output_contexts;

	// Create output streams
	void CreateStreams();

	// Delete output streams
	void DeleteStreams();

	// Send frame with output stream's information
	void SendFrame(std::shared_ptr<MediaPacket> packet);
};

