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
	
	// Codec 
	void SetCodecId(cmn::MediaCodecId id);
	cmn::MediaCodecId GetCodecId() const;

	// Specific Codec Library ID
	void SetCodecLibraryId(cmn::MediaCodecLibraryId id);
	cmn::MediaCodecLibraryId GetCodecLibraryId() const;

	// Variant Name (used for rendition of playlist)
	void SetVariantName(const ov::String &name);
	ov::String GetVariantName() const;

	// Public Name (used for multiple audio/video tracks. e.g. multilingual audio)
	void SetPublicName(const ov::String &name);
	ov::String GetPublicName() const;

	// Language (rfc5646)
	void SetLanguage(const ov::String &language);
	ov::String GetLanguage() const;

	// Media Type 
	void SetMediaType(cmn::MediaType type);
	cmn::MediaType GetMediaType() const;

	// Origin bitstream foramt
	void SetOriginBitstream(cmn::BitstreamFormat format);
	cmn::BitstreamFormat GetOriginBitstream() const;

	// Bypass 
	void SetBypass(bool flag);
	bool IsBypass() const;

	// Bypass (Set by user)
	void SetBypassByConfig(bool flag);
	bool IsBypassByConf() const;

	// Timebase 
	const cmn::Timebase &GetTimeBase() const;
	void SetTimeBase(int32_t num, int32_t den);
	void SetTimeBase(const cmn::Timebase &time_base);

	// Bitrate 
	// Return the proper bitrate for this track. 
	// If there is a bitrate set by the user, it is returned. If not, the automatically measured bitrate is returned	
	int32_t GetBitrate() const;

	// Bitrate (Set by measured)
	void SetBitrateByMeasured(int32_t bitrate);
	int32_t GetBitrateByMeasured() const;

	// Bitrate (Set by user)
	void SetBitrateByConfig(int32_t bitrate);
	int32_t GetBitrateByConfig() const;
	
	// Frame Time 
	void SetStartFrameTime(int64_t time);
	int64_t GetStartFrameTime() const;
	void SetLastFrameTime(int64_t time);
	int64_t GetLastFrameTime() const;

	bool IsValid();
	bool HasQualityMeasured();

	// Define extradata by codec
	//  H264 : AVCDecoderConfigurationRecord
	//  H265 : HEVCDecoderConfiguratinRecord(TODO)
	//  AAC : AACSpecificConfig
	//  VP8, Opus : Unused
	void SetCodecExtradata(const std::shared_ptr<ov::Data> &codec_extradata);
	const std::shared_ptr<ov::Data> &GetCodecExtradata() const;
	std::shared_ptr<ov::Data> &GetCodecExtradata();

	// For statistics
	void OnFrameAdded(uint64_t bytes);

	int64_t GetTotalFrameCount() const;
	int64_t GetTotalFrameBytes() const;

	std::shared_ptr<MediaTrack> Clone();

	ov::String GetInfoString();

protected: 

	// Track ID
	uint32_t _id;

	// Media Type
	cmn::MediaType _media_type;

	// Codec
	cmn::MediaCodecId _codec_id;
	cmn::MediaCodecLibraryId _codec_library_id;

	// Variant Name : Original encoder profile that made this track 
	// from <OutputProfile><Encodes>(<Video> || <Audio> || <Image>)<Name>
	ov::String _variant_name;

	// Set by AudioMap or VideoMap
	ov::String _public_name;
	ov::String _language;

	// Bitstream format 
	cmn::BitstreamFormat _origin_bitstream_format = cmn::BitstreamFormat::Unknown;

	// Timebase
	cmn::Timebase _time_base;

	// Bitrate
	int32_t _bitrate;
	// Bitrate (Set by user)
	int32_t _bitrate_conf;
	
	// Bypass
	bool _byass;
	// Bypass (Set by user)
	bool _bypass_conf;

	// Time of start frame(packet)
	int64_t _start_frame_time;

	// Time of last frame(packet)
	int64_t _last_frame_time;

	// Extradata
	std::shared_ptr<ov::Data> _codec_extradata = nullptr;

	// First frame received time
	ov::StopWatch _clock_from_first_frame_received;

	// Statistics
	uint64_t _total_frame_count = 0;
	uint64_t _total_frame_bytes = 0;

	// Validity
	bool _is_valid = false;


	bool _has_quality_measured = false;
};
