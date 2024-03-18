#include "transcoder_filter.h"

#include "filter/filter_resampler.h"
#include "filter/filter_rescaler.h"
#include "transcoder_gpu.h"
#include "transcoder_private.h"

using namespace cmn;

#define PTS_INCREMENT_LIMIT 15

TranscodeFilter::TranscodeFilter()
	: _internal(nullptr)
{
}

TranscodeFilter::~TranscodeFilter()
{
}

bool TranscodeFilter::Configure(int32_t id,
								const std::shared_ptr<info::Stream>& input_stream_info, std::shared_ptr<MediaTrack> input_track,
								const std::shared_ptr<info::Stream>& output_stream_info, std::shared_ptr<MediaTrack> output_track,
								CompleteHandler complete_handler)
{
	_id = id;
	_input_stream_info = input_stream_info;
	_input_track = input_track;
	_output_stream_info = output_stream_info;
	_output_track = output_track;
	_complete_handler = complete_handler;

	_timestamp_jump_threshold = (int64_t)_input_track->GetTimeBase().GetTimescale() * PTS_INCREMENT_LIMIT;

	return Create();
}

bool TranscodeFilter::Create()
{
	std::lock_guard<std::shared_mutex> lock(_mutex);

	// If there is a previously created filter, remove it.
	if (_internal != nullptr)
	{
		_internal->Stop();
		_internal.reset();
		_internal = nullptr;
	}

	switch (_input_track->GetMediaType())
	{
		case MediaType::Audio:
			_internal = std::make_shared<FilterResampler>();
			break;
		case MediaType::Video:
			_internal = std::make_shared<FilterRescaler>();
			break;
		default:
			logte("Unsupported media type in filter");
			return false;
	}

	auto name = ov::String::FormatString("filter_%s", cmn::GetMediaTypeString(_input_track->GetMediaType()).CStr());
	auto urn = std::make_shared<info::ManagedQueue::URN>(
		_input_stream_info->GetApplicationName(),
		_input_stream_info->GetName(),
		"trs",
		name.LowerCaseString());
	_internal->SetQueueUrn(urn);
	_internal->SetCompleteHandler(bind(&TranscodeFilter::OnComplete, this, std::placeholders::_1));

	bool success = _internal->Configure(_input_track, _output_track);
	if (success == false)
	{
		logte("Could not create filter");

		return false;
	}

	return _internal->Start();
}

void TranscodeFilter::Stop()
{
	std::lock_guard<std::shared_mutex> lock(_mutex);

	if (_internal != nullptr)
	{
		_internal->Stop();
		_internal.reset();
		_internal = nullptr;
	}
}

bool TranscodeFilter::SendBuffer(std::shared_ptr<MediaFrame> buffer)
{
	if (IsNeedUpdate(buffer) == true)
	{
		if (Create() == false)
		{
			logte("Failed to regenerate filter");
			return false;
		}

		return true;
	}

	std::shared_lock<std::shared_mutex> lock(_mutex);
	if (_internal == nullptr)
	{
		return false;
	}

	return _internal->SendBuffer(std::move(buffer));
}

bool TranscodeFilter::IsNeedUpdate(std::shared_ptr<MediaFrame> buffer)
{
	// In case of pts/dts jumps
	int64_t last_timestamp = _last_timestamp;
	int64_t curr_timestamp = buffer->GetPts();
	_last_timestamp = curr_timestamp;

	// Check #1 - Abnormal timestamp
	int64_t increment = abs(curr_timestamp - last_timestamp);
	bool is_abnormal_timestamp = (last_timestamp != -1LL && increment > _timestamp_jump_threshold) ? true : false;
	if (is_abnormal_timestamp)
	{
		logtw("Timestamp has changed abnormally.  %lld -> %lld", last_timestamp, buffer->GetPts());

		return true;
	}

	// Check #2 - Resolution change
	std::shared_lock<std::shared_mutex> lock(_mutex);
	
	if (_internal == nullptr)
	{
		return false;
	}

	if (_input_track->GetMediaType() == MediaType::Video)
	{
		if (buffer->GetWidth() != (int32_t)_internal->GetInputWidth() ||
			buffer->GetHeight() != (int32_t)_internal->GetInputHeight())
		{
			logti("Changed input resolution of %u track. (%dx%d -> %dx%d)", _input_track->GetId(), _internal->GetInputWidth(), _internal->GetInputHeight(), buffer->GetWidth(), buffer->GetHeight());
			_input_track->SetWidth(buffer->GetWidth());
			_input_track->SetHeight(buffer->GetHeight());
			return true;
		}
	}

	// When using an XMA scaler, resource allocation failures may occur intermittently.
	// Avoid problems in this way until the underlying problem is resolved.
	if (_internal->GetState() == FilterBase::State::ERROR &&
		_input_track->GetCodecModuleId() == cmn::MediaCodecModuleId::XMA &&
		_output_track->GetCodecModuleId() == cmn::MediaCodecModuleId::XMA)
	{
		logtw("It is assumed that the XMA resource allocation failed. So, recreate the filter.");
		return true;
	}

	return false;
}

void TranscodeFilter::SetCompleteHandler(CompleteHandler complete_handler)
{
	_complete_handler = move(complete_handler);
}

void TranscodeFilter::OnComplete(std::shared_ptr<MediaFrame> frame)
{
	if (_complete_handler)
	{
		_complete_handler(_id, frame);
	}
}

cmn::Timebase TranscodeFilter::GetInputTimebase() const
{
	return _internal->GetInputTimebase();
}

cmn::Timebase TranscodeFilter::GetOutputTimebase() const
{
	return _internal->GetOutputTimebase();
}

std::shared_ptr<MediaTrack>& TranscodeFilter::GetInputTrack()
{
	return _input_track;
}

std::shared_ptr<MediaTrack>& TranscodeFilter::GetOutputTrack()
{
	return _output_track;
}