//==============================================================================
//
//  TranscoderStream
//
//  Created by Keukhan
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <stdint.h>

#include <memory>
#include <queue>
#include <vector>

#include "base/info/application.h"
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

		void AddOutput(std::shared_ptr<info::Stream> s, std::shared_ptr<MediaTrack> t)
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
	static std::shared_ptr<TranscoderStream> Create(const info::Application &application_info, const std::shared_ptr<info::Stream> &stream, TranscodeApplication *parent);

	TranscoderStream(const info::Application &application_info, const std::shared_ptr<info::Stream> &orig_stream, TranscodeApplication *parent);
	~TranscoderStream();

	info::stream_id_t GetStreamId();

	bool Start();
	bool Stop();
	bool Prepare(const std::shared_ptr<info::Stream> &stream);
	bool Update(const std::shared_ptr<info::Stream> &stream);
	bool Push(std::shared_ptr<MediaPacket> packet);

	// Notify event to mediarouter
	void NotifyCreateStreams();
	void NotifyDeleteStreams();
	void NotifyUpdateStreams();

private:
	// Create stream --> Start stream --> Stop stream --> Delete stream
	enum class State : uint8_t
	{
		CREATED = 0,
		PREPARING,
		STARTED,
		STOPPED,
		ERROR,
	};

	// Set the stream state
	void SetState(State state)
	{
		_state = state;
	}

	State GetState() const
	{
		return _state;
	}

	State _state = State::CREATED;

private:
	ov::String _log_prefix;
	std::shared_mutex _format_change_mutex;
	std::shared_mutex _decoder_map_mutex;
	std::shared_mutex _filter_map_mutex;
	std::shared_mutex _encoder_map_mutex;

	TranscodeApplication *_parent;

	const info::Application _application_info;

	// Output profile settings. It is used as an external profile or local profile depending on the webhook result.
	const cfg::vhost::app::oprf::OutputProfiles *GetOutputProfilesCfg()
	{
		return _output_profiles_cfg;
	}
	const cfg::vhost::app::oprf::OutputProfiles *_output_profiles_cfg;
	// Output profile set from webhook
	cfg::vhost::app::oprf::OutputProfiles _remote_output_profiles;

	// Input Stream Info
	std::shared_ptr<info::Stream> _input_stream;

	// Output Stream Info
	// [OUTPUT_STREAM_NAME, OUTPUT_stream]
	std::map<ov::String, std::shared_ptr<info::Stream>> _output_streams;

	// Map of CompositeContext
	// Purpose of reusing the same encoding profile.
	// 
	// [
	//   - MAP_ID
	// 	 - INPUT  -> STREAM and TRACK
	// 	 - OUTPUTS -> STREAM + TRACK
	// ]
	std::map<std::pair<ov::String, cmn::MediaType>, std::shared_ptr<CompositeContext>> _composite_map;
	std::atomic<MediaTrackId> _last_composite_id = 0;

	// This map is used only when the Passthrough options is enabled.
	// [INPUT_TRACK_ID,  OUTPUT_TRACK_IDS of OutputStream]
	std::map<MediaTrackId, std::vector<std::pair<std::shared_ptr<info::Stream>, MediaTrackId>>> _link_input_to_outputs;

	// [INPUT_TRACK_ID, DECODER_ID]
	std::map<MediaTrackId, MediaTrackId> _link_input_to_decoder;

	// [DECODER_ID, FILTER_IDS]
	std::map<MediaTrackId, std::vector<MediaTrackId>> _link_decoder_to_filters;

	// [FILTER_ID, ENCODER_ID]
	std::map<MediaTrackId, MediaTrackId> _link_filter_to_encoder;

	// [ENCODER_ID, OUTPUT_TRACK_IDS Of OutputStream]
	std::map<MediaTrackId, std::vector<std::pair<std::shared_ptr<info::Stream>, MediaTrackId>>> _link_encoder_to_outputs;

	// Decoder Component
	// [DECODER_ID, DECODER]
	std::map<MediaTrackId, std::shared_ptr<TranscodeDecoder>> _decoders;

	// Last decoded frame and timestamp
	// [DECODER_ID, MediaFrame]
	std::map<MediaTrackId, std::shared_ptr<MediaFrame>> _last_decoded_frames;
	// [DECODER_ID, Timestamp(microseconds)]
	std::map<MediaTrackId, int64_t> _last_decoded_frame_pts;

	// Filter Component
	// [FILTER_ID, FILTER]
	std::map<MediaTrackId, std::shared_ptr<TranscodeFilter>> _filters;

	// Encoder Component
	// [ENCODER_ID, ENCODER]
	std::map<MediaTrackId, std::shared_ptr<TranscodeEncoder>> _encoders;

private:
	std::shared_ptr<MediaTrack> GetInputTrack(MediaTrackId track_id);
	std::shared_ptr<info::Stream> GetInputStream();
	std::shared_ptr<info::Stream> GetOutputStreamByTrackId(MediaTrackId output_track_id);

	const cfg::vhost::app::oprf::OutputProfiles* RequestWebhoook();
	bool StartInternal();
	bool PrepareInternal();
	bool UpdateInternal(const std::shared_ptr<info::Stream> &stream);

	int32_t CreateOutputStreamDynamic();
	int32_t CreateOutputStreams();
	std::shared_ptr<info::Stream> CreateOutputStream(const cfg::vhost::app::oprf::OutputProfile &cfg_output_profile);

	int32_t BuildComposite();
	// Store information for track mapping by stage
	void AddComposite(ov::String serialized_profile,
					  std::shared_ptr<info::Stream> input_stream, std::shared_ptr<MediaTrack> input_track,
					  std::shared_ptr<info::Stream> output_stream, std::shared_ptr<MediaTrack> output_track);
	ov::String GetInfoStringComposite();

	int32_t CreateDecoders();
	bool CreateDecoder(MediaTrackId decoder_id, std::shared_ptr<info::Stream> input_stream, std::shared_ptr<MediaTrack> input_track);

	int32_t CreateFilters(MediaFrame *buffer);
	bool CreateFilter(MediaTrackId filter_id, std::shared_ptr<MediaTrack> input_track, std::shared_ptr<MediaTrack> output_track);
	std::shared_ptr<MediaTrack> GetInputTrackOfFilter(MediaTrackId decoder_id);

	int32_t CreateEncoders(MediaFrame *buffer);
	bool CreateEncoder(MediaTrackId encoder_id, std::shared_ptr<info::Stream> output_stream, std::shared_ptr<MediaTrack> output_track);

	// Step 1: Decode (Decode a frame from given packets)
	void DecodePacket(const std::shared_ptr<MediaPacket> &packet);
	void OnDecodedFrame(TranscodeResult result, MediaTrackId decoder_id, std::shared_ptr<MediaFrame> decoded_frame);
	void SetLastDecodedFrame(MediaTrackId decoder_id, std::shared_ptr<MediaFrame> &decoded_frame);
	std::shared_ptr<MediaFrame> GetLastDecodedFrame(MediaTrackId decoder_id);

	// Called when formatting of decoded frames is analyzed or changed.
	void ChangeOutputFormat(MediaFrame *buffer);
	void UpdateInputTrack(MediaFrame *buffer);
	void UpdateOutputTrack(MediaFrame *buffer);
	void UpdateMsidOfOutputStreams(uint32_t msid);

	// Step 2: Filter (resample/rescale the decoded frame)
	void SpreadToFilters(MediaTrackId decoder_id, std::shared_ptr<MediaFrame> frame);
	TranscodeResult FilterFrame(MediaTrackId track_id, std::shared_ptr<MediaFrame> frame);
	void OnFilteredFrame(MediaTrackId filter_id, std::shared_ptr<MediaFrame> decoded_frame);
	bool IsAvailableSmoothTransition(const std::shared_ptr<info::Stream> &stream);

	// Step 3: Encode (Encode the filtered frame to packets)
	TranscodeResult EncodeFrame(std::shared_ptr<const MediaFrame> frame);
	void OnEncodedPacket(MediaTrackId encoder_id, std::shared_ptr<MediaPacket> encoded_packet);

	// Send encoded packet to mediarouter via transcoder application
	void SendFrame(std::shared_ptr<info::Stream> &stream, std::shared_ptr<MediaPacket> packet);

	// Remove all components
	void RemoveAllComponents();
	void RemoveDecoders();
	void RemoveFilters();
	void RemoveEncoders();

private:
	// Initial buffer for ready to stream
	void BufferMediaPacketUntilReadyToPlay(const std::shared_ptr<MediaPacket> &media_packet);
	bool SendBufferedPackets();
	ov::Queue<std::shared_ptr<MediaPacket>> _initial_media_packet_buffer;
};
