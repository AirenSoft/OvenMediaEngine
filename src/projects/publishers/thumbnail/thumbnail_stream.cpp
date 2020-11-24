#include "thumbnail_private.h"
#include "thumbnail_stream.h"
#include "base/publisher/application.h"
#include "base/publisher/stream.h"
#include <regex>

std::shared_ptr<ThumbnailStream> ThumbnailStream::Create(const std::shared_ptr<pub::Application> application,
											 const info::Stream &info)
{
	auto stream = std::make_shared<ThumbnailStream>(application, info);
	if(!stream->Start())
	{
		return nullptr;
	}

	if(!stream->CreateStreamWorker(2))
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
		GetApplicationName() , GetName().CStr());
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
	auto stream_packet = std::make_any<std::shared_ptr<MediaPacket>>(media_packet);

	BroadcastPacket(stream_packet);
}

void ThumbnailStream::SendAudioFrame(const std::shared_ptr<MediaPacket> &media_packet)
{
	auto stream_packet = std::make_any<std::shared_ptr<MediaPacket>>(media_packet);

	BroadcastPacket(stream_packet);	
}

std::shared_ptr<ThumbnailSession> ThumbnailStream::CreateSession()
{
	auto session = ThumbnailSession::Create(GetApplication(), GetSharedPtrAs<pub::Stream>(), this->IssueUniqueSessionId());
	if(session == nullptr)
	{
		logte("Internal Error : Cannot create session");
		return nullptr;
	}

	logtd("created session");

	AddSession(session);

	return session;
}

bool ThumbnailStream::DeleteSession(uint32_t session_id)
{
	return RemoveSession(session_id);
}
