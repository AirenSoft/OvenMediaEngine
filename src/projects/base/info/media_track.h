//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "video_track.h"
#include "audio_track.h"

#define VALID_BITRATE_CALCULATION_THRESHOLD_MSEC (1000)

typedef uint32_t MediaTrackId;

class MediaTrack : public VideoTrack, public AudioTrack
{
public:
	MediaTrack();
	MediaTrack(const MediaTrack &media_track);
	~MediaTrack();

	// Track ID
	void SetId(uint32_t id);
	uint32_t GetId() const;

	// Variant Name (used for rendition of playlist)
	void SetVariantName(const ov::String &name);
	ov::String GetVariantName() const;

	// Public Name (used for multiple audio/video tracks. e.g. multilingual audio)
	void SetPublicName(const ov::String &name);
	ov::String GetPublicName() const;

	// Language (rfc5646)
	void SetLanguage(const ov::String &language);
	ov::String GetLanguage() const;

	// Video Type Settings
	void SetMediaType(cmn::MediaType type);
	cmn::MediaType GetMediaType() const;

	// Codec Settings
	void SetCodecId(cmn::MediaCodecId id);
	cmn::MediaCodecId GetCodecId() const;

	void SetCodecLibraryId(cmn::MediaCodecLibraryId id);
	cmn::MediaCodecLibraryId GetCodecLibraryId() const;

	// Origin bitstream foramt
	void SetOriginBitstream(cmn::BitstreamFormat format);
	cmn::BitstreamFormat GetOriginBitstream() const;

	// Timebase Settings
	const cmn::Timebase &GetTimeBase() const;
	void SetTimeBase(int32_t num, int32_t den);
	void SetTimeBase(const cmn::Timebase &time_base);

	// Bitrate Settings
	void SetBitrate(int32_t bitrate);
	int32_t GetBitrate() const;

	// Frame Time Settings
	void SetStartFrameTime(int64_t time);
	int64_t GetStartFrameTime() const;

	void SetLastFrameTime(int64_t time);
	int64_t GetLastFrameTime() const;

	// Bypass Settings
	void SetBypass(bool flag);
	bool IsBypass() const;

	ov::String GetInfoString();
	bool IsValid();
	bool HasQualityMeasured();

	// Define extradata by codec
	//  H264 : AVCDecoderConfigurationRecord
	//  H265 : HEVCDecoderConfiguratinRecord(TODO)
	//  AAC : AACSpecificConfig
	//  VP8 : No plan
	//  OPUS : No plan
	void SetCodecExtradata(const std::shared_ptr<ov::Data> &codec_extradata);
	const std::shared_ptr<ov::Data> &GetCodecExtradata() const;
	std::shared_ptr<ov::Data> &GetCodecExtradata();

	// For statistics
	void OnFrameAdded(uint64_t bytes);

private:
	bool _is_valid = false;
	bool _has_quality_measured = false;

	uint32_t _id;

	// Variant Name : Original encoder profile that made this track 
	// from <OutputProfile><Encodes>(<Video> || <Audio> || <Image>)<Name>
	ov::String _variant_name;

	// Set by AudioMap or VideoMap
	ov::String _public_name;
	ov::String _language;

	cmn::MediaCodecId _codec_id;
	cmn::MediaCodecLibraryId _codec_library_id;
	cmn::BitstreamFormat _origin_bitstream_format = cmn::BitstreamFormat::Unknown;
	cmn::MediaType _media_type;
	cmn::Timebase _time_base;
	int32_t _bitrate;

	bool	_byass;

	// Time of start frame(packet)
	int64_t _start_frame_time;

	// Time of last frame(packet)
	int64_t _last_frame_time;

	std::shared_ptr<ov::Data> _codec_extradata = nullptr;

	// Statistics

	// First frame received time
	ov::StopWatch _clock_from_first_frame_received;

	uint64_t _total_frame_count = 0;
	uint64_t _total_frame_bytes = 0;

public:
	void SetHardwareAccel(bool hwaccel);
	bool GetHardwareAccel() const;
	bool _use_hwaccel;
};
