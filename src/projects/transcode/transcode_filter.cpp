#include "transcode_filter.h"

#include "filter/media_filter_resampler.h"
#include "filter/media_filter_rescaler.h"

using namespace common;


#define OV_LOG_TAG "TranscodeFilter"

TranscodeFilter::TranscodeFilter() :
	_impl(nullptr)
{
	// av_log_set_level(AV_LOG_TRACE);
}

TranscodeFilter::TranscodeFilter(std::shared_ptr<MediaTrack> input_media_track, std::shared_ptr<TranscodeContext> context) :
	_impl(nullptr)
{
	Configure(input_media_track, context);
}

TranscodeFilter::~TranscodeFilter()
{
	if(_impl != nullptr)
	{
		delete _impl;
	}
}

bool TranscodeFilter::Configure(std::shared_ptr<MediaTrack> input_media_track, std::shared_ptr<TranscodeContext> context)
{
	MediaType type = input_media_track->GetMediaType();

	switch(type)
	{
		case MediaType::Audio:
			_impl = new MediaFilterResampler();
			break;
		case MediaType::Video:
			_impl = new MediaFilterRescaler();
			break;
		default:
			logte("Unsupported filter. type(%d)", type);
			return false;
	}

	input_media_track->GetTimeBase().Set(1, 1000);

	// 트랜스코딩 컨텍스트 정보 전달
	_impl->Configure(std::move(input_media_track), std::move(context));

	return true;
}

int32_t TranscodeFilter::SendBuffer(std::unique_ptr<MediaFrame> buffer)
{
	return _impl->SendBuffer(std::move(buffer));
}

std::unique_ptr<MediaFrame> TranscodeFilter::RecvBuffer(TranscodeResult *result)
{
	return _impl->RecvBuffer(result);
}
