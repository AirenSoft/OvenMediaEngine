#pragma once

#include "transcode_context.h"
#include "filter/media_filter_impl.h"
#include "codec/transcode_base.h"

#include <cstdint>

#include <base/media_route/media_buffer.h>
#include <base/media_route/media_type.h>

enum class TranscodeFilterType : int8_t
{
	None = -1,
	AudioResampler,
	VideoRescaler,

	Count           ///< Number of sample formats. DO NOT USE if linking dynamically
};

class TranscodeFilter
{
public:
	TranscodeFilter();
	TranscodeFilter(std::shared_ptr<MediaTrack> input_media_track = nullptr, std::shared_ptr<TranscodeContext> context = nullptr);
	~TranscodeFilter();

	bool Configure(std::shared_ptr<MediaTrack> input_media_track = nullptr, std::shared_ptr<TranscodeContext> context = nullptr);

	int32_t SendBuffer(std::unique_ptr<MediaFrame> buffer);
	std::unique_ptr<MediaFrame> RecvBuffer(TranscodeResult *result);

private:
	MediaFilterImpl *_impl;
};

