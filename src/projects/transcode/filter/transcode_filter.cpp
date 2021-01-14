#include "transcode_filter.h"

#include "../transcode_private.h"
#include "media_filter_resampler.h"
#include "media_filter_rescaler.h"

using namespace cmn;

TranscodeFilter::TranscodeFilter()
	: _impl(nullptr)
{
}

TranscodeFilter::TranscodeFilter(std::shared_ptr<MediaTrack> input_media_track, std::shared_ptr<TranscodeContext> input_context, std::shared_ptr<TranscodeContext> output_context)
	: _impl(nullptr)
{
	// Configure(input_media_track, std::move(input_context), std::move(output_context));
	Configure(input_media_track, input_context, output_context);
}

TranscodeFilter::~TranscodeFilter()
{
	if (_impl != nullptr)
	{
		delete _impl;
	}
}

bool TranscodeFilter::Configure(std::shared_ptr<MediaTrack> input_media_track, std::shared_ptr<TranscodeContext> input_context, std::shared_ptr<TranscodeContext> output_context)
{
	logtd("Create a transcode filter. track_id(%d). type(%s)", input_media_track->GetId(), (input_media_track->GetMediaType() == MediaType::Video) ? "Video" : "Audio");

	MediaType type = input_media_track->GetMediaType();
	switch (type)
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

	// 트랜스코딩 컨텍스트 정보 전달
	_impl->Configure(input_media_track, input_context, output_context);
	return true;
}

int32_t TranscodeFilter::SendBuffer(std::shared_ptr<MediaFrame> buffer)
{
	return _impl->SendBuffer(std::move(buffer));
}

std::shared_ptr<MediaFrame> TranscodeFilter::RecvBuffer(TranscodeResult *result)
{
	return _impl->RecvBuffer(result);
}

uint32_t TranscodeFilter::GetInputBufferSize()
{
	return _impl->GetInputBufferSize();
}

uint32_t TranscodeFilter::GetOutputBufferSize()
{
	return _impl->GetOutputBufferSize();
}

cmn::Timebase TranscodeFilter::GetInputTimebase() const
{
	return _impl->GetInputTimebase();
}

cmn::Timebase TranscodeFilter::GetOutputTimebase() const
{
	return _impl->GetOutputTimebase();
}