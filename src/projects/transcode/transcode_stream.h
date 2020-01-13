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

	// std::set<ov::String> _stream_list;

	// For statistics
	uint32_t _stats_decoded_frame_count;
	uint8_t _stats_queue_full_count;
	uint64_t _max_queue_size;

private:
	const info::Application _application_info;

	// Input Stream Info 
	std::shared_ptr<StreamInfo> _stream_info_input;

	// Output Stream Info
	// [OUTPUT_STREAM_NAME, OUTPUT_STREAM_INFO]
	std::map<ov::String, std::shared_ptr<StreamInfo>> _stream_info_outputs;

	// Map with track ID. Maps from one input track to multiple output tracks.
	// [INPUT_TRACK_ID, [OUTPUT_TRACK_ID,MediaTrack] ARRAY]
	std::map <uint8_t, std::vector<std::pair<uint8_t,std::shared_ptr<MediaTrack>>> > _tracks_map;

	// Decoder
	// INPUT_TRACK_ID, DECODER
	std::map<MediaTrackId, std::shared_ptr<TranscodeDecoder>> _decoders;

	// Filter
	// OUTPUT_TRACK_ID, FILTER
	std::map<MediaTrackId, std::shared_ptr<TranscodeFilter>> _filters;

	// Encoder
	// OUTPUT_TRACK_ID, ENCODER
	std::map<MediaTrackId, std::shared_ptr<TranscodeEncoder>> _encoders;


	// Buffer for encoded(input) media packets
	MediaQueue<std::shared_ptr<MediaPacket>> _queue_input_packets;

	// Buffer for decoded frames
	MediaQueue<std::shared_ptr<MediaFrame>> _queue_decoded_frames;

	// Buffer for filtered frames
	MediaQueue<std::shared_ptr<MediaFrame>> _queue_filterd_frames;

	// last generated output track id.
	uint8_t _last_track_index = 0;

	volatile bool _kill_flag;

	void LoopTask();
	std::thread _thread_looptask;

	TranscodeApplication *_parent;

	int32_t CreateOutputStream();

	int32_t CreateDecoders();
	bool CreateDecoder(int32_t track_id, std::shared_ptr<TranscodeContext> input_context);

	int32_t CreateEncoders();
	bool CreateEncoder(std::shared_ptr<MediaTrack> media_track, std::shared_ptr<TranscodeContext> output_context);

	// 디코딩된 프레임의 포맷이 분석되거나 변경될 경우 호출됨.
	void ChangeOutputFormat(MediaFrame *buffer);

	void CreateFilters(MediaFrame *buffer);
	void DoFilters(std::shared_ptr<MediaFrame> frame);

	// There are 3 steps to process packet
	// Step 1: Decode (Decode a frame from given packets)
	TranscodeResult DecodePacket(int32_t track_id, std::shared_ptr<MediaPacket> packet);
	// Step 2: Filter (resample/rescale the decoded frame)
	TranscodeResult FilterFrame(int32_t track_id, std::shared_ptr<MediaFrame> frame);
	// Step 3: Encode (Encode the filtered frame to packets)
	TranscodeResult EncodeFrame(int32_t track_id, std::shared_ptr<const MediaFrame> frame);

	// Transcoding information
	uint8_t NewTrackId(common::MediaType media_type);

	// Create output streams
	void CreateStreams();

	// Delete output streams
	void DeleteStreams();

	// Send frame with output stream's information
	void SendFrame(std::shared_ptr<MediaPacket> packet);

	void GetByassTrackInfo(int32_t track_id, int32_t& bypass, int32_t& non_bypass);

	const cfg::Encode* GetEncodeByProfileName(const info::Application &application_info, ov::String encode_name);
	common::MediaCodecId GetCodecId(ov::String name);
	int GetBitrate(ov::String bitrate)	;
};

