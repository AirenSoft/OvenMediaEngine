//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include <map>
#include <vector>
#include <deque>
#include <mutex>
#include <string.h>
#include "../base/ovlibrary/ovlibrary.h"
#include "bit_writer.h"

#define PACKTYZER_DEFAULT_TIMESCALE                (90000)//90MHz
#define AVC_NAL_START_PATTERN_SIZE    (4) //0x00000001
#define ADTS_HEADER_SIZE            (7)

#pragma pack(1)

enum class PacketyzerStreamType : int32_t {
    Common,        // Video + Audio
    VideoOnly,
    AudioOnly,
};

enum class PacketyzerType : int32_t {
    Dash,
    Hls,
};

enum class PlayListType : int32_t {
    M3u8,
    Mpd,
};

enum class SegmentType : int32_t {
    MpegTs,
    M4S,
};

enum class SegmentDataType : int32_t {
    Ts,        // Video + Audio
    Mp4Video,
    Mp4Audio,

};

//====================================================================================================
// SegmentData
//====================================================================================================
struct SegmentData
{
public :

    SegmentData(int sequence_number_,
           ov::String file_name_,
            uint64_t duration_,
            uint64_t timestamp_,
            const std::shared_ptr<std::vector<uint8_t>> &data_)
    {
        sequence_number = sequence_number_;
        file_name = file_name_;
        create_time = time(nullptr);
        duration = duration_;
        timestamp = timestamp_;
        data = std::make_shared<ov::Data>(data_->data(), data_->size());
    }

    SegmentData(int sequence_number_,
            ov::String &file_name_,
            uint64_t duration_,
            uint64_t timestamp_,
            std::shared_ptr<ov::Data> &data_)
    {
        sequence_number = sequence_number_;
        file_name = file_name_;
        create_time = time(nullptr);
        duration = duration_;
        timestamp = timestamp_;
        data = data_;
    }

public :
    int sequence_number;
    ov::String file_name;
    time_t create_time;
    uint64_t duration;
    uint64_t timestamp;
    std::shared_ptr<ov::Data> data;
};

//====================================================================================================
// PacketyzerFrameData
//====================================================================================================
enum class PacketyzerFrameType {
    VideoIFrame = 'I', // Key
    VideoPFrame = 'P',
    VideoBFrame = 'B',
    AudioFrame = 'A',
};

//====================================================================================================
// PacketyzerFrameData
//====================================================================================================
struct PacketyzerFrameData
{
public:
    PacketyzerFrameData(PacketyzerFrameType type_,
                        uint64_t timestamp_,
                        uint64_t time_offset_,
                        uint32_t time_scale_,
                        std::shared_ptr<ov::Data> &data_)
    {
        type = type_;
        timestamp = timestamp_;
        time_offset = time_offset_;
        timescale = time_scale_;
        data = data_;
    }

    PacketyzerFrameData(PacketyzerFrameType type_, uint64_t timestamp_, uint64_t time_offset_, uint32_t time_scale_)
    {
        type = type_;
        timestamp = timestamp_;
        time_offset = time_offset_;
        timescale = time_scale_;
        data = std::make_shared<ov::Data>();
    }

public:
    PacketyzerFrameType type;
    uint64_t timestamp;
    uint64_t time_offset;
    uint32_t timescale;
    std::shared_ptr<ov::Data> data;
};


//===============================================================================================
//지원 코덱 타입
//===============================================================================================
enum class SegmentCodecType
{
    UnknownCodec,
    Vp8Codec,
    H264Codec,
    OpusCodec,
    AacCodec,
};

//====================================================================================================
// Interface
//====================================================================================================
struct PacketyzerMediaInfo
{
public :
    PacketyzerMediaInfo()
    {
        video_codec_type = SegmentCodecType::UnknownCodec;
        video_width = 0;
        video_height = 0;
        video_framerate = 0;
        video_bitrate = 0;
        video_timescale = 0;

        audio_codec_type = SegmentCodecType::UnknownCodec;
        audio_channels = 0;
        audio_samplerate = 0;
        audio_bitrate = 0;
        audio_timescale = 0;
    }

    PacketyzerMediaInfo(SegmentCodecType video_codec_type_,
                        uint32_t video_width_,
                        uint32_t video_height_,
                        float video_framerate_,
                        uint32_t video_bitrate_,
                        uint32_t video_timescale_,
                        SegmentCodecType audio_codec_type_,
                        uint32_t audio_channels_,
                        uint32_t audio_samplerate_,
                        uint32_t audio_bitrate_,
                        uint32_t audio_timescale_)
    {
        // 비디오 정보
        video_codec_type = video_codec_type_;
        video_width = video_width_;
        video_height = video_height_;
        video_framerate = video_framerate_;
        video_bitrate = video_bitrate_;
        video_timescale = video_timescale_;

        // 오디오 정보
        audio_codec_type = audio_codec_type_;
        audio_channels = audio_channels_;
        audio_samplerate = audio_samplerate_;
        audio_bitrate = audio_bitrate_;
        audio_timescale = audio_timescale_;
    }

public :
    // 비디오 정보
    SegmentCodecType video_codec_type;
    uint32_t video_width;
    uint32_t video_height;
    float video_framerate;
    uint32_t video_bitrate;
    uint32_t video_timescale;

    // 오디오 정보
    SegmentCodecType audio_codec_type;
    uint32_t audio_channels;
    uint32_t audio_samplerate;
    uint32_t audio_bitrate;
    uint32_t audio_timescale;
};

#pragma pack()
