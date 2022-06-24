#include "transcoder_filter.h"

#include "filter/filter_resampler.h"
#include "filter/filter_rescaler.h"
#include "transcoder_gpu.h"
#include "transcoder_private.h"

using namespace cmn;

#define PTS_INCREMENT_LIMIT 15

TranscodeFilter::TranscodeFilter()
	: _impl(nullptr)
{
}

TranscodeFilter::~TranscodeFilter()
{
	if (_impl != nullptr)
	{
		delete _impl;
	}
}

bool TranscodeFilter::Configure(int32_t filter_id, std::shared_ptr<MediaTrack> input_media_track, std::shared_ptr<TranscodeContext> input_context, std::shared_ptr<TranscodeContext> output_context, _cb_func on_complete_hander)
{
	logtd("Create a transcode filter. track_id(%d). type(%s)", input_media_track->GetId(), (input_media_track->GetMediaType() == MediaType::Video) ? "Video" : "Audio");

	_filter_id = filter_id;
	_input_media_track = input_media_track;
	_input_context = input_context;
	_output_context = output_context;
	_on_complete_hander = on_complete_hander;
	_threshold_ts_increment = (int64_t)_input_media_track->GetTimeBase().GetTimescale() * PTS_INCREMENT_LIMIT;

	return CreateFilter();
}

bool TranscodeFilter::CreateFilter()
{
	if (_impl != nullptr)
	{
		delete _impl;
	}

	switch (_input_media_track->GetMediaType())
	{
		case MediaType::Audio:
			_impl = new FilterResampler();
			break;
		case MediaType::Video:
			_impl = new FilterRescaler();
			break;
		default:
			logte("Unsupported media type in filter");
			return false;
	}

	_impl->SetOnCompleteHandler(bind(&TranscodeFilter::OnComplete, this, std::placeholders::_1));

	bool success = _impl->Configure(_input_media_track, _input_context, _output_context);
	if (success == false)
	{
		logte("Could not craete filter");

		return false;
	}

	return _impl->Start();
}

void TranscodeFilter::Stop() {
	if(_impl)
	{
		_impl->Stop();
	}
}

bool TranscodeFilter::SendBuffer(std::shared_ptr<MediaFrame> buffer)
{
	if (IsNeedUpdate(buffer) == true)
	{
		if (CreateFilter() == false)
		{
			logte("Failed to regenerate filter");

			return false;
		}
	}

	return (_impl->SendBuffer(std::move(buffer)) == 0) ? true : false;
}

bool TranscodeFilter::IsNeedUpdate(std::shared_ptr<MediaFrame> buffer)
{
	//	If the timestamp increases rapidly, out of memory occurs while padding is performed in the fps and aresample filters.
	// To prevent this, the filter is regenerated.
	int64_t ts_increment = abs(buffer->GetPts() - _last_pts);
	int64_t tmp_last_pts = _last_pts;
	bool detect_abnormal_increace_pts = (_last_pts != -1LL && ts_increment > _threshold_ts_increment) ? true : false;

	_last_pts = buffer->GetPts();

	if (detect_abnormal_increace_pts)
	{
		logtw("Timestamp has changed abnormally.  %lld -> %lld", tmp_last_pts, buffer->GetPts());

		return true;
	}

	if (_input_media_track->GetMediaType() == MediaType::Video)
	{
		if (buffer->GetWidth() != (int32_t)_input_context->GetVideoWidth() || buffer->GetHeight() != (int32_t)_input_context->GetVideoHeight())
		{
			_input_media_track->SetWidth(buffer->GetWidth());
			_input_media_track->SetHeight(buffer->GetHeight());
			_input_context->SetVideoWidth(buffer->GetWidth());
			_input_context->SetVideoHeight(buffer->GetHeight());
			logti("Changed resolution %u track", _input_media_track->GetId());
			return true;
		}
	}

	return false;
}

void TranscodeFilter::OnComplete(std::shared_ptr<MediaFrame> frame) {
	if(_on_complete_hander)
	{
		_on_complete_hander(_filter_id, frame);
	}
}

cmn::Timebase TranscodeFilter::GetInputTimebase() const
{
	return _impl->GetInputTimebase();
}

cmn::Timebase TranscodeFilter::GetOutputTimebase() const
{
	return _impl->GetOutputTimebase();
}

void TranscodeFilter::SetAlias(ov::String alias)
{
	_alias = alias;
}
