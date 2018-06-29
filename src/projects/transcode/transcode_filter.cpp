#include "transcode_filter.h"

#include "filter/media_filter_resampler.h"
#include "filter/media_filter_rescaler.h"

using namespace MediaCommonType;


#define OV_LOG_TAG "TranscodeFilter"

TranscodeFilter::TranscodeFilter() :
	_impl(NULL)
{
	// av_log_set_level(AV_LOG_TRACE);
}

TranscodeFilter::TranscodeFilter(FilterType type, std::shared_ptr<MediaTrack> input_media_track, std::shared_ptr<TranscodeContext> context) :
	_impl(NULL)
{
	Configure(type, input_media_track, context);
}

TranscodeFilter::~TranscodeFilter()
{
	if(_impl != nullptr)
		delete _impl;
}


int32_t TranscodeFilter::Configure(FilterType type, std::shared_ptr<MediaTrack> input_media_track, std::shared_ptr<TranscodeContext> context)
{


	switch(type)
	{
	case FilterType::FILTER_TYPE_AUDIO_RESAMPLER:
		_impl = new MediaFilterResampler();
	break;
	case FilterType::FILTER_TYPE_VIDEO_RESCALER:
		_impl = new MediaFilterRescaler();
	break;
	default:
		logte("unsupport filter. type(%d)", type);
		return 1;
	}	

	// 트랜스코딩 컨텍스트 정보 전달
	_impl->Configure(input_media_track, context);

	return 0;

}

int32_t TranscodeFilter::SendBuffer(std::unique_ptr<MediaBuffer> buf)
{
	return _impl->SendBuffer(std::move(buf));
}

std::pair<int32_t, std::unique_ptr<MediaBuffer>> TranscodeFilter::RecvBuffer()
{
	auto obj = _impl->RecvBuffer();

	return obj;
}
