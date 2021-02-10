//==============================================================================
//
//  TranscodeStream
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/info/application.h>

#include <cstdint>
#include <memory>
#include <queue>
#include <vector>

#include "base/info/stream.h"
#include "base/mediarouter/media_buffer.h"
#include "base/mediarouter/media_type.h"
#include "codec/transcode_decoder.h"
#include "codec/transcode_encoder.h"
#include "transcode_context.h"
#include "filter/transcode_filter.h"

typedef int32_t MediaTrackId;

class TranscodeApplication;

class TranscodeTrackMapContext
{
public:
	MediaTrackId _map_id;

	// Input Track
	std::shared_ptr<info::Stream> _input_stream;
	std::shared_ptr<MediaTrack> _input_track;

	// Output Tracks
	std::vector<std::pair<std::shared_ptr<info::Stream>, std::shared_ptr<MediaTrack>>> _output_tracks;
};

class TranscodeStream : public ov::EnableSharedFromThis<TranscodeStream>
{
public:
	TranscodeStream(const info::Application &application_info, const std::shared_ptr<info::Stream> &orig_stream, TranscodeApplication *parent);
	~TranscodeStream();

	info::stream_id_t GetStreamId();

	bool Start();
	bool Stop();

	bool Push(std::shared_ptr<MediaPacket> packet);

private:
	// ov::Semaphore _queue_event;

	const info::Application _application_info;

	// Input Stream Info
	std::shared_ptr<info::Stream> _input_stream;

	// Output Stream Info
	// [OUTPUT_STREAM_NAME, OUTPUT_stream]
	std::map<ov::String, std::shared_ptr<info::Stream>> _output_streams;

	// Store information for track mapping by stage
	void AppendTrackMap(ov::String unique_id,
						std::shared_ptr<info::Stream> input_stream,
						std::shared_ptr<MediaTrack> input_track,
						std::shared_ptr<info::Stream> output_stream,
						std::shared_ptr<MediaTrack> output_track);

	std::map<std::pair<ov::String, cmn::MediaType>, std::shared_ptr<TranscodeTrackMapContext>> _track_map;
	MediaTrackId _last_map_id;

	// Input Track -> Decoder or Router(bypasS)
	// [INPUT_TRACK, DECODER_ID]
	std::map<MediaTrackId, MediaTrackId> _stage_input_to_decoder;

	// [INPUT_TRACK, Output Stream + Track Id]
	std::map<MediaTrackId, std::vector<std::pair<std::shared_ptr<info::Stream>, MediaTrackId>>> _stage_input_to_output;

	// [DECODER_ID, FILTER_ID(trasncode_id)]
	std::map<MediaTrackId, std::vector<MediaTrackId>> _stage_decoder_to_filter;

	// [FILTER_ID(trasncode_id), ENCODER_ID(trasncode_id)]
	std::map<MediaTrackId, MediaTrackId> _stage_filter_to_encoder;

	// [ENCODER_ID(trasncode_id), OUTPUT_TRACKS]
	std::map<MediaTrackId, std::vector<std::pair<std::shared_ptr<info::Stream>, MediaTrackId>>> _stage_encoder_to_output;

	// Decoder
	// DECODR_ID, DECODER
	std::map<MediaTrackId, std::shared_ptr<TranscodeDecoder>> _decoders;

	// Filter
	// FILTER_ID, FILTER
	std::map<MediaTrackId, std::shared_ptr<TranscodeFilter>> _filters;

	// Encoder
	// ENCODER_ID, ENCODER
	std::map<MediaTrackId, std::shared_ptr<TranscodeEncoder>> _encoders;

	// last generated output track id.
	uint8_t _last_track_index = 0;

	volatile bool _kill_flag;

	TranscodeApplication *GetParent();
	TranscodeApplication *_parent;

	bool IsSupportedMediaType(const std::shared_ptr<MediaTrack> &media_track);
	int32_t CreateOutputStreams();
	std::shared_ptr<info::Stream> CreateOutputStream(const cfg::vhost::app::oprf::OutputProfile &cfg_output_profile);
	std::shared_ptr<MediaTrack> CreateOutputTrack(const std::shared_ptr<MediaTrack> &input_track, const cfg::vhost::app::oprf::VideoProfile &profile);
	std::shared_ptr<MediaTrack> CreateOutputTrack(const std::shared_ptr<MediaTrack> &input_track, const cfg::vhost::app::oprf::AudioProfile &profile);
	std::shared_ptr<MediaTrack> CreateOutputTrack(const std::shared_ptr<MediaTrack> &input_track, const cfg::vhost::app::oprf::ImageProfile &profile);
	uint8_t NewTrackId(cmn::MediaType media_type);

	// for dynamically generated applications
	int32_t CreateOutputStreamDynamic();

	int32_t CreateStageMapping();

	int32_t CreateDecoders();
	bool CreateDecoder(int32_t input_track_id, int32_t decoder_track_id, std::shared_ptr<TranscodeContext> input_context);

	void CreateFilter(MediaFrame *buffer);

	int32_t CreateEncoders(MediaTrackId track_id);
	bool CreateEncoder(int32_t encoder_track_id, std::shared_ptr<TranscodeContext> output_context);

	// Called when formatting of decoded frames is analyzed or changed.
	void ChangeOutputFormat(MediaFrame *buffer);
	void UpdateInputTrack(MediaFrame *buffer);
	void UpdateOutputTrack(MediaFrame *buffer);
	void UpdateDecoderContext(MediaTrackId track_id);


	// There are 3 steps to process packet
	// Step 1: Decode (Decode a frame from given packets)
	void DecodePacket(int32_t track_id, std::shared_ptr<MediaPacket> packet);
	void OnDecodedPacket(TranscodeResult result, int32_t decoder_id);

	// Step 2: Filter (resample/rescale the decoded frame)
	void SpreadToFilters(std::shared_ptr<MediaFrame> frame);
	TranscodeResult FilterFrame(int32_t track_id, std::shared_ptr<MediaFrame> frame);

	// Step 3: Encode (Encode the filtered frame to packets)
	TranscodeResult EncodeFrame(int32_t track_id, std::shared_ptr<const MediaFrame> frame);
	TranscodeResult OnEncodedPacket(int32_t encoder_id);


	// Send frame with output stream's information
	void SendFrame(std::shared_ptr<info::Stream> &stream, std::shared_ptr<MediaPacket> packet);

public:
	cmn::MediaCodecId GetCodecId(ov::String name);

	bool IsVideoCodec(cmn::MediaCodecId codec_id);
	bool IsAudioCodec(cmn::MediaCodecId codec_id);

	ov::String GetIdentifiedForVideoProfile(const cfg::vhost::app::oprf::VideoProfile &profile);
	ov::String GetIdentifiedForAudioProfile(const cfg::vhost::app::oprf::AudioProfile &profile);
	ov::String GetIdentifiedForImageProfile(const cfg::vhost::app::oprf::ImageProfile &profile);

	const cmn::Timebase GetDefaultTimebaseByCodecId(cmn::MediaCodecId codec_id);

	// Create output streams
	void NotifyCreateStreams();

	// Delete output streams
	void NotifyDeleteStreams();
};
