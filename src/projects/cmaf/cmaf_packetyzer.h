//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include "segment_stream/packetyzer/packetyzer.h"
#include "segment_stream/packetyzer/m4s_init_writer.h"
#include "segment_stream/packetyzer/cmaf_chunk_writer.h"

//====================================================================================================
// Cmaf chunked transfer notify interface
//====================================================================================================
class ICmafChunkedTransfer
{
public :
	// chunk data push notify
	virtual void OnCmafChunkDataPush(const ov::String &app_name,
										const ov::String &stream_name,
										const ov::String &file_name,
										uint32_t chunk_index, // 0 base
										std::shared_ptr<ov::Data> &chunk_data) = 0;// 1frame chunk

	// chunked segment completed notify
	virtual void OnCmafChunkedComplete(const ov::String &app_name,
									   const ov::String &stream_name,
									   const ov::String &file_name) = 0;


};

//====================================================================================================
// CmafPacketyzer
// m4s : [Prefix]_[Index]_[Suffix].m4s
// mpd : manifest.mpd
// completed segment data manager --> CmafStreamServer
//====================================================================================================
class CmafPacketyzer : public Packetyzer
{
public:
    CmafPacketyzer(const ov::String &app_name,
					const ov::String &stream_name,
					PacketyzerStreamType stream_type,
					const ov::String &segment_prefix,
					uint32_t segment_count,
					uint32_t segment_duration,
					PacketyzerMediaInfo &media_info,
				    const std::shared_ptr<ICmafChunkedTransfer> &chunked_transfer);

    ~CmafPacketyzer() final;

public :
    bool VideoInit(const std::shared_ptr<ov::Data> &frame_data);

    bool AudioInit();

    virtual bool AppendVideoFrame(std::shared_ptr<PacketyzerFrameData> &frame_data) override;

    virtual bool AppendAudioFrame(std::shared_ptr<PacketyzerFrameData> &frame_data) override;

    bool VideoSegmentWrite(uint32_t current_number, uint64_t max_timestamp);

    bool AudioSegmentWrite(uint32_t current_number, uint64_t max_timestamp);

    virtual const std::shared_ptr<SegmentData> GetSegmentData(const ov::String &file_name) override;

	virtual bool GetPlayList(ov::String &play_list) override;

    virtual bool SetSegmentData(ov::String file_name,
								uint64_t duration,
								uint64_t timestamp,
								std::shared_ptr<ov::Data> &data) override;


protected :
    bool UpdatePlayList();

	ov::String GetCurrentVideoFileName();
	ov::String GetCurrentAudioFileName();

private :
    int _avc_nal_header_size;

    double _available_start_time = 0;

    uint64_t _video_start_timestamp = 0;
	uint64_t _video_last_timestamp = 0;
	double _video_last_tick = 0;

    uint64_t _audio_start_timestamp = 0;
	uint64_t _audio_last_timestamp = 0;
	double _audio_last_tick = 0;

    std::shared_ptr<SegmentData> _video_init_file = nullptr;
    std::shared_ptr<SegmentData> _audio_init_file = nullptr;

    uint32_t _video_sequence_number = 1;
    uint32_t _audio_sequence_number = 1;

    std::shared_ptr<PacketyzerFrameData> _previous_video_frame = nullptr;
    std::shared_ptr<PacketyzerFrameData> _previous_audio_frame = nullptr;

    std::unique_ptr<CmafChunkWriter> _video_chunk_writer = nullptr;
    std::unique_ptr<CmafChunkWriter> _audio_chunk_writer = nullptr;

	std::shared_ptr<ICmafChunkedTransfer> _chunked_transfer = nullptr;

	bool _is_video_time_base = false;
	bool _video_play_list_update = false;
	bool _audio_play_list_update = false;
};