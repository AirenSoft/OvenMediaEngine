#pragma once

#include <stdint.h>

#include "filter/media_filter_impl.h"
#include "base/media_route/media_buffer.h"
#include "base/media_route/media_type.h"
#include "transcode_context.h"


class TranscodeFilter
{
public:
	   enum class FilterType : int8_t 
        {
            FILTER_TYPE_NONE = -1,
            FILTER_TYPE_AUDIO_RESAMPLER,
            FILTER_TYPE_VIDEO_RESCALER,
            FILTER_TYPE_NB           ///< Number of sample formats. DO NOT USE if linking dynamically
        };

public:
    TranscodeFilter();
    TranscodeFilter(FilterType type, std::shared_ptr<MediaTrack> input_media_track = nullptr, std::shared_ptr<TranscodeContext> context = nullptr);
    ~TranscodeFilter();

    int32_t Configure(FilterType type, std::shared_ptr<MediaTrack> input_media_track = nullptr, std::shared_ptr<TranscodeContext> context = nullptr); 

    int32_t SendBuffer(std::unique_ptr<MediaBuffer> buf);

    std::pair<int32_t, std::unique_ptr<MediaBuffer>> RecvBuffer();
    
private:
	MediaFilterImpl* 	_impl;
};

