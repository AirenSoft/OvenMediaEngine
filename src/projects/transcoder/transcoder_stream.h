//==============================================================================
//
//  TranscoderStream
//
//  Created by Keukhan
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
#include "transcoder_context.h"
#include "transcoder_decoder.h"
#include "transcoder_encoder.h"
#include "transcoder_filter.h"
#include "transcoder_stream_internal.h"

class TranscodeApplication;

class TranscoderStream : public ov::EnableSharedFromThis<TranscoderStream>, public TranscoderStreamInternal
{
public:
	class CompositeContext
	{
	public:
		CompositeContext(MediaTrackId id)
			: _id(id)
		{
		}

		MediaTrackId GetId()
		{
			return _id;
		}

		void SetInput(std::shared_ptr<info::Stream> s, std::shared_ptr<MediaTrack> t)
		{
			_input_track = std::make_pair(s, t);
		}

		void InsertOutput(std::shared_ptr<info::Stream> s, std::shared_ptr<MediaTrack> t)
		{
			_output_tracks.push_back(std::make_pair(s, t));
		}

		std::shared_ptr<info::Stream> &GetInputStream()
		{
			return _input_track.first;
		}

		std::shared_ptr<MediaTrack> &GetInputTrack()
		{
			return _input_track.second;
		}

		std::vector<std::pair<std::shared_ptr<info::Stream>, std::shared_ptr<MediaTrack>>> &GetOutputTracks()
		{
			return _output_tracks;
		}

	private:
		MediaTrackId _id;

		// Input Track
		std::pair<std::shared_ptr<info::Stream>, std::shared_ptr<MediaTrack>> _input_track;

		// Output Tracks
		std::vector<std::pair<std::shared_ptr<info::Stream>, std::shared_ptr<MediaTrack>>> _output_tracks;
	};

public:
	TranscoderStream(const info::Application &application_info, const std::shared_ptr<info::Stream> &orig_stream, TranscodeApplication *parent);
	~TranscoderStream();

	info::stream_id_t GetStreamId();

	bool Start();
	bool Stop();
	bool Prepare();
	bool Update(const std::shared_ptr<info::Stream> &stream);
	bool Push(std::shared_ptr<MediaPacket> packet);

	// Notify event to mediarouter
	void NotifyCreateStreams();
	void NotifyDeleteStreams();
	void NotifyUpdateStreams();

private:
	ov::String _log_prefix;
	mutable std::mutex _mutex;

	TranscodeApplication *_parent;

	const info::Application _application_info;

	// Input Stream Info
	std::shared_ptr<info::Stream> _input_stream;

	// Output Stream Info
	// [OUTPUT_STREAM_NAME, OUTPUT_stream]
	std::map<ov::String, std::shared_ptr<info::Stream>> _output_streams;

private:
	// Store information for track mapping by stage
	void AddCompositeMap(ov::String unique_id,
						 std::shared_ptr<info::Stream> input_stream,
						 std::shared_ptr<MediaTrack> input_track,
						 std::shared_ptr<info::Stream> output_stream,
						 std::shared_ptr<MediaTrack> output_track);

	// Map of CompositeContext
	// [
	//   - MAP_ID
	// 	 - INPUT  -> STREAM and TRACK
	// 	 - OUTPUTS -> STREAM + TRACK
	// ]
	std::map<std::pair<ov::String, cmn::MediaType>, std::shared_ptr<CompositeContext>> _composite_map;
	std::atomic<MediaTrackId> _last_map_id = 0;

	// [INPUT_TRACK, Output Stream + Track Id]
	std::map<MediaTrackId, std::vector<std::pair<std::shared_ptr<info::Stream>, MediaTrackId>>> _stage_input_to_outputs;

	// [INPUT_TRACK, DECODER_ID]
	std::map<MediaTrackId, MediaTrackId> _stage_input_to_decoder;

	// [DECODER_ID, FILTER_ID]
	std::map<MediaTrackId, std::vector<MediaTrackId>> _stage_decoder_to_filters;

	// [FILTER_ID, ENCODER_ID]
	std::map<MediaTrackId, MediaTrackId> _stage_filter_to_encoder;

	// [ENCODER_ID, OUTPUT_TRACKS]
	std::map<MediaTrackId, std::vector<std::pair<std::shared_ptr<info::Stream>, MediaTrackId>>> _stage_encoder_to_outputs;

	// Decoder Component
	// DECODR_ID, DECODER
	std::map<MediaTrackId, std::shared_ptr<TranscodeDecoder>> _decoders;
	std::map<MediaTrackId, std::shared_ptr<MediaFrame>> _last_decoded_frames;

	// Filter Component
	// FILTER_ID, FILTER
	std::map<MediaTrackId, std::shared_ptr<TranscodeFilter>> _filters;

	// Encoder Component
	// ENCODER_ID, ENCODER
	std::map<MediaTrackId, std::shared_ptr<TranscodeEncoder>> _encoders;

	// Last timestamp decoded frame. 
	// DECODER_ID, Timestamp(microseconds)
	std::map<MediaTrackId, int64_t> _last_decoded_frame_pts;


	std::shared_ptr<MediaTrack> GetInputTrack(MediaTrackId track_id);

	int32_t CreateOutputStreamDynamic();
	int32_t CreateOutputStreams();
	std::shared_ptr<info::Stream> CreateOutputStream(const cfg::vhost::app::oprf::OutputProfile &cfg_output_profile);
	std::shared_ptr<MediaTrack> CreateOutputTrack(const std::shared_ptr<MediaTrack> &input_track, const cfg::vhost::app::oprf::VideoProfile &profile);
	std::shared_ptr<MediaTrack> CreateOutputTrack(const std::shared_ptr<MediaTrack> &input_track, const cfg::vhost::app::oprf::AudioProfile &profile);
	std::shared_ptr<MediaTrack> CreateOutputTrack(const std::shared_ptr<MediaTrack> &input_track, const cfg::vhost::app::oprf::ImageProfile &profile);
	std::shared_ptr<MediaTrack> CreateOutputTrackDataType(const std::shared_ptr<MediaTrack> &input_track);
	
	int32_t CreatePipeline();

	int32_t CreateDecoders();
	bool CreateDecoder(int32_t decoder_id, std::shared_ptr<MediaTrack> input_track);

	void CreateFilters(MediaFrame *buffer);
	std::shared_ptr<MediaTrack> GetFilterInputContext(int32_t decoder_id);

	int32_t CreateEncoders(MediaFrame *buffer);
	bool CreateEncoder(int32_t encoder_id, std::shared_ptr<MediaTrack> output_track);

	// Called when formatting of decoded frames is analyzed or changed.
	void ChangeOutputFormat(MediaFrame *buffer);
	void UpdateInputTrack(MediaFrame *buffer);
	void UpdateOutputTrack(MediaFrame *buffer);

	// There are 3 steps to process packet
	// Step 1: Decode (Decode a frame from given packets)
	void DecodePacket(std::shared_ptr<MediaPacket> packet);
	void OnDecodedFrame(TranscodeResult result, int32_t decoder_id, std::shared_ptr<MediaFrame> decoded_frame);
	void SetLastDecodedFrame(int32_t decoder_id, std::shared_ptr<MediaFrame> &decoded_frame);
	std::shared_ptr<MediaFrame> GetLastDecodedFrame(int32_t decoder_id);

	// Step 2: Filter (resample/rescale the decoded frame)
	void SpreadToFilters(int32_t decoder_id, std::shared_ptr<MediaFrame> frame);
	TranscodeResult FilterFrame(int32_t track_id, std::shared_ptr<MediaFrame> frame);
	void OnFilteredFrame(int32_t filter_id, std::shared_ptr<MediaFrame> decoded_frame);

	// Step 3: Encode (Encode the filtered frame to packets)
	TranscodeResult EncodeFrame(std::shared_ptr<const MediaFrame> frame);
	void OnEncodedPacket(int32_t encoder_id, std::shared_ptr<MediaPacket> encoded_packet);

	// Send encoded packet to mediarouter via transcoder application
	void SendFrame(std::shared_ptr<info::Stream> &stream, std::shared_ptr<MediaPacket> packet);

	void RemoveAllComponents();
	void RemoveDecoders();
	void RemoveFilters();
	void RemoveEncoders();

	void UpdateMsidOfOutputStreams(uint32_t msid);
	bool IsAvailableSmoothTransitionStream(const std::shared_ptr<info::Stream> &stream);
};
