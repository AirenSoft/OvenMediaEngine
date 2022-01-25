#include "rtmppush_stream.h"

#include <regex>

#include "base/publisher/application.h"
#include "base/publisher/stream.h"
#include "rtmppush_application.h"
#include "rtmppush_private.h"

std::shared_ptr<RtmpPushStream> RtmpPushStream::Create(const std::shared_ptr<pub::Application> application,
													   const info::Stream &info)
{
	auto stream = std::make_shared<RtmpPushStream>(application, info);
	return stream;
}

RtmpPushStream::RtmpPushStream(const std::shared_ptr<pub::Application> application,
							   const info::Stream &info)
	: Stream(application, info)
{
}

RtmpPushStream::~RtmpPushStream()
{
	logtd("RtmpPushStream(%s/%s) has been terminated finally",
		  GetApplicationName(), GetName().CStr());
}

bool RtmpPushStream::Start()
{
	if (GetState() != Stream::State::CREATED)
	{
		return false;
	}

	if (!CreateStreamWorker(2))
	{
		return false;
	}

	logtd("RtmpPushStream(%ld) has been started", GetId());

	std::static_pointer_cast<RtmpPushApplication>(GetApplication())->SessionController();

	return Stream::Start();
}

bool RtmpPushStream::Stop()
{
	logtd("RtmpPushStream(%u) has been stopped", GetId());

	if (GetState() != Stream::State::STARTED)
	{
		return false;
	}

	std::static_pointer_cast<RtmpPushApplication>(GetApplication())->SessionController();

	return Stream::Stop();
}

void RtmpPushStream::SendVideoFrame(const std::shared_ptr<MediaPacket> &media_packet)
{
	if (GetState() != Stream::State::STARTED)
	{
		return;
	}

	auto stream_packet = std::make_any<std::shared_ptr<MediaPacket>>(media_packet);

	BroadcastPacket(stream_packet);
	MonitorInstance->IncreaseBytesOut(*pub::Stream::GetSharedPtrAs<info::Stream>(), PublisherType::RtmpPush, media_packet->GetData()->GetLength() * GetSessionCount());
}

void RtmpPushStream::SendAudioFrame(const std::shared_ptr<MediaPacket> &media_packet)
{
	if (GetState() != Stream::State::STARTED)
	{
		return;
	}

	auto stream_packet = std::make_any<std::shared_ptr<MediaPacket>>(media_packet);

	BroadcastPacket(stream_packet);
	MonitorInstance->IncreaseBytesOut(*pub::Stream::GetSharedPtrAs<info::Stream>(), PublisherType::RtmpPush, media_packet->GetData()->GetLength() * GetSessionCount());
}

std::shared_ptr<RtmpPushSession> RtmpPushStream::CreateSession()
{
	auto session = RtmpPushSession::Create(GetApplication(), GetSharedPtrAs<pub::Stream>(), this->IssueUniqueSessionId());
	if (session == nullptr)
	{
		logte("Internal Error : Cannot create session");
		return nullptr;
	}

	AddSession(session);

	return session;
}

bool RtmpPushStream::DeleteSession(uint32_t session_id)
{
	return RemoveSession(session_id);
}
