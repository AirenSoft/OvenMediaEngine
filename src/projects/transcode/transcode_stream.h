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

#include "base/mediarouter/media_buffer.h"
#include "base/mediarouter/media_type.h"
#include "base/info/stream.h"

#include "transcode_context.h"
#include "transcode_filter.h"

#include "codec/transcode_encoder.h"
#include "codec/transcode_decoder.h"

#include <base/info/application.h>

typedef int32_t MediaTrackId;

class TranscodeApplication;

class TranscodeStageContext
{
public:
	MediaTrackId _transcoder_id;
	std::shared_ptr<MediaTrack> _input_track;
	std::vector< std::pair<std::shared_ptr<info::Stream>, std::shared_ptr<MediaTrack>> > _output_tracks;
};

class TranscodeStream : public ov::EnableSharedFromThis<TranscodeStream>
{
public:
	TranscodeStream(const info::Application &application_info, const std::shared_ptr<info::Stream> &orig_stream, TranscodeApplication *parent);
	~TranscodeStream();

	bool Start();
	bool Stop();

	bool Push(std::shared_ptr<MediaPacket> packet);

	// For statistics
	uint64_t 	_max_queue_threshold;

	void DoInputPackets();
	void DoDecodedFrames();
	void DoFilteredFrames();
	
private:
	// ov::Semaphore _queue_event;

	const info::Application _application_info;

	// Input Stream Info
	std::shared_ptr<info::Stream> _input_stream;

	// Output Stream Info
	// [OUTPUT_STREAM_NAME, OUTPUT_stream]
	std::map<ov::String, std::shared_ptr<info::Stream>> _stream_outputs;


	// Store information for track mapping by stage
	void StoreStageContext(ov::String unique_id, std::shared_ptr<MediaTrack> input_track, std::shared_ptr<info::Stream> output_stream, std::shared_ptr<MediaTrack> output_track);
	std::map< std::pair<ov::String, cmn::MediaType>, std::shared_ptr<TranscodeStageContext> > _map_stage_context;
	MediaTrackId _last_transcode_id;

	// Input Track -> Decoder or Router(bypasS)
	// [INPUT_TRACK, DECODER_ID]
	std::map <MediaTrackId, MediaTrackId> _stage_input_to_decoder;
	
	// [INPUT_TRACK, Output Stream + Track Id]
	std::map <MediaTrackId, std::vector<std::pair<std::shared_ptr<info::Stream>, MediaTrackId>> > _stage_input_to_output;

	// [DECODER_ID, FILTER_ID(trasncode_id)]
	std::map <MediaTrackId, std::vector<MediaTrackId> > _stage_decoder_to_filter;

	// [FILTER_ID(trasncode_id), ENCODER_ID(trasncode_id)]
	std::map <MediaTrackId, MediaTrackId> _stage_filter_to_encoder;

	// [ENCODER_ID(trasncode_id), OUTPUT_TRACKS]
	std::map <MediaTrackId, std::vector<std::pair<std::shared_ptr<info::Stream>, MediaTrackId>>> _stage_encoder_to_output;


	// Decoder
	// DECODR_ID, DECODER
	std::map<MediaTrackId, std::shared_ptr<TranscodeDecoder>> _decoders;

	// Filter
	// FILTER_ID, FILTER
	std::map<MediaTrackId, std::shared_ptr<TranscodeFilter>> _filters;

	// Encoder
	// ENCODER_ID, ENCODER
	std::map<MediaTrackId, std::shared_ptr<TranscodeEncoder>> _encoders;


	// Buffer for encoded(input) media packets
	ov::Queue<std::shared_ptr<MediaPacket>> _queue_input_packets;

	// Buffer for decoded frames
	ov::Queue<std::shared_ptr<MediaFrame>> _queue_decoded_frames;

	// Buffer for filtered frames
	ov::Queue<std::shared_ptr<MediaFrame>> _queue_filterd_frames;


	// last generated output track id.
	uint8_t _last_track_index = 0;

	volatile bool _kill_flag;

	TranscodeApplication* GetParent();
	TranscodeApplication* _parent;

	bool IsSupportedMediaType(const std::shared_ptr<MediaTrack> &media_track);
	int32_t CreateOutputStreams();
	std::shared_ptr<info::Stream> CreateOutputStream(const cfg::vhost::app::oprf::OutputProfile &cfg_output_profile);
	std::shared_ptr<MediaTrack> CreateOutputTrack(const std::shared_ptr<MediaTrack>& input_track, const cfg::vhost::app::oprf::VideoProfile &profile);
	std::shared_ptr<MediaTrack> CreateOutputTrack(const std::shared_ptr<MediaTrack>& input_track, const cfg::vhost::app::oprf::AudioProfile &profile);

	// for dynamically generated applications
	int32_t CreateOutputStreamDynamic();

	int32_t CreateStageMapping();

	int32_t CreateDecoders();
	bool CreateDecoder(int32_t input_track_id, int32_t decoder_track_id, std::shared_ptr<TranscodeContext> input_context);

	int32_t CreateEncoders();
	bool CreateEncoder(int32_t encoder_track_id, std::shared_ptr<MediaTrack> media_track, std::shared_ptr<TranscodeContext> output_context);

	// Called when formatting of decoded frames is analyzed or changed.
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
	TranscodeResult EncodedPacket(int32_t encoder_id);

	// Transcoding information
	uint8_t NewTrackId(cmn::MediaType media_type);

	// Create output streams
	void CreateStreams();

	// Delete output streams
	void DeleteStreams();

	// Send frame with output stream's information
	void SendFrame(std::shared_ptr<info::Stream> &stream, std::shared_ptr<MediaPacket> packet);

	// const cfg::vhost::app::enc::Encode *GetEncodeByProfileName(const info::Application &application_info, ov::String encode_name);

	cmn::MediaCodecId GetCodecId(ov::String name);

	bool IsVideoCodec(cmn::MediaCodecId codec_id);
	bool IsAudioCodec(cmn::MediaCodecId codec_id);

	ov::String GetIdentifiedForVideoProfile(const cfg::vhost::app::oprf::VideoProfile &profile);
	ov::String GetIdentifiedForAudioProfile(const cfg::vhost::app::oprf::AudioProfile &profile);

	const cmn::Timebase GetDefaultTimebaseByCodecId(cmn::MediaCodecId codec_id);
};

