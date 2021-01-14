#pragma once

#include <base/mediarouter/media_buffer.h>
#include <base/mediarouter/media_type.h>

#include <cstdint>

#include "../transcode_context.h"
// #include "codec/transcode_base.h"
#include "media_filter_impl.h"

enum class TranscodeFilterType : int8_t
{
	None = -1,
	AudioResampler,
	VideoRescaler,

	Count  ///< Number of sample formats. DO NOT USE if linking dynamically
};

class TranscodeFilter
{
public:
	TranscodeFilter();
	TranscodeFilter(std::shared_ptr<MediaTrack> input_media_track, std::shared_ptr<TranscodeContext> input_context, std::shared_ptr<TranscodeContext> output_context);
	~TranscodeFilter();

	bool Configure(std::shared_ptr<MediaTrack> input_media_track, std::shared_ptr<TranscodeContext> input_context, std::shared_ptr<TranscodeContext> output_context);

	int32_t SendBuffer(std::shared_ptr<MediaFrame> buffer);
	std::shared_ptr<MediaFrame> RecvBuffer(TranscodeResult *result);

	uint32_t GetInputBufferSize();
	uint32_t GetOutputBufferSize();

	cmn::Timebase GetInputTimebase() const;
	cmn::Timebase GetOutputTimebase() const;

private:
	MediaFilterImpl *_impl;
};
