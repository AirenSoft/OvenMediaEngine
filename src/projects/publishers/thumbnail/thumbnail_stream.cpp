#include "thumbnail_stream.h"

#include <regex>

#include "base/publisher/application.h"
#include "base/publisher/stream.h"
#include "thumbnail_private.h"

std::shared_ptr<ThumbnailStream> ThumbnailStream::Create(const std::shared_ptr<pub::Application> application,
														 const info::Stream &info)
{
	auto stream = std::make_shared<ThumbnailStream>(application, info);
	if (!stream->Start())
	{
		return nullptr;
	}

	if (!stream->CreateStreamWorker(2))
	{
		return nullptr;
	}

	return stream;
}

ThumbnailStream::ThumbnailStream(const std::shared_ptr<pub::Application> application,
								 const info::Stream &info)
	: Stream(application, info)
{
}

ThumbnailStream::~ThumbnailStream()
{
	logtd("ThumbnailStream(%s/%s) has been terminated finally",
		  GetApplicationName(), GetName().CStr());
}

bool ThumbnailStream::Start()
{
	logtd("ThumbnailStream(%ld) has been started", GetId());

	return Stream::Start();
}

bool ThumbnailStream::Stop()
{
	logtd("ThumbnailStream(%u) has been stopped", GetId());

	return Stream::Stop();
}

void ThumbnailStream::SendVideoFrame(const std::shared_ptr<MediaPacket> &media_packet)
{
	auto track = GetTrack(media_packet->GetTrackId());
	if (track == nullptr)
	{
		logtw("Could not find track. id: %d", media_packet->GetTrackId());
		return;
	}

	if (!(track->GetCodecId() == cmn::MediaCodecId::Png || track->GetCodecId() == cmn::MediaCodecId::Jpeg))
	{
		// Could not support codec for image
		return;
	}

	std::lock_guard<std::shared_mutex> lock(_encoded_frame_mutex);

	_encoded_frames[track->GetCodecId()] = std::move(media_packet->GetData()->Clone());
}

void ThumbnailStream::SendAudioFrame(const std::shared_ptr<MediaPacket> &media_packet)
{
	// Nothing..
}

std::shared_ptr<ov::Data> ThumbnailStream::GetVideoFrameByCodecId(cmn::MediaCodecId codec_id)
{
	std::shared_lock<std::shared_mutex> lock(_encoded_frame_mutex);

	auto it = _encoded_frames.find(codec_id);
	if (it == _encoded_frames.end())
	{
		return nullptr;
	}

	return it->second;
}