#pragma once

#include <base/mediarouter/media_buffer.h>
#include <base/mediarouter/media_type.h>

#include <cstdint>

#include "filter/filter_base.h"
#include "transcoder_context.h"

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

	bool SendBuffer(std::shared_ptr<MediaFrame> buffer);
	std::shared_ptr<MediaFrame> RecvBuffer(TranscodeResult *result);

	uint32_t GetInputBufferSize();
	uint32_t GetOutputBufferSize();

	cmn::Timebase GetInputTimebase() const;
	cmn::Timebase GetOutputTimebase() const;

	int64_t _last_pts = -1LL;
	int64_t _threshold_ts_increment = 0LL;

	std::shared_ptr<MediaTrack> _input_media_track;
	std::shared_ptr<TranscodeContext> _input_context;
	std::shared_ptr<TranscodeContext> _output_context;

private:
	bool CreateFilter();
	bool IsNeedUpdate(std::shared_ptr<MediaFrame> buffer);

	MediaFilterImpl *_impl;
};
