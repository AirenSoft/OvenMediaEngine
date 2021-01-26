#include "file_stream.h"

#include <regex>

#include "base/publisher/application.h"
#include "base/publisher/stream.h"
#include "file_private.h"

std::shared_ptr<FileStream> FileStream::Create(const std::shared_ptr<pub::Application> application,
											   const info::Stream &info)
{
	auto stream = std::make_shared<FileStream>(application, info);
	return stream;
}

FileStream::FileStream(const std::shared_ptr<pub::Application> application,
					   const info::Stream &info)
	: Stream(application, info)
{
}

FileStream::~FileStream()
{
	logtd("FileStream(%s/%s) has been terminated finally",
		  GetApplicationName(), GetName().CStr());
}

bool FileStream::Start()
{
	if(GetState() != Stream::State::CREATED)
	{
		return false;
	}

	logtd("FileStream(%ld) has been started", GetId());

	if (!CreateStreamWorker(2))
	{
		return false;
	}

	return Stream::Start();
}

bool FileStream::Stop()
{
	if(GetState() != Stream::State::STARTED)
	{
		return false;
	}

	logtd("FileStream(%u) has been stopped", GetId());

	return Stream::Stop();
}

void FileStream::SendVideoFrame(const std::shared_ptr<MediaPacket> &media_packet)
{
	if(GetState() != Stream::State::STARTED)
	{
		return;
	}

	auto stream_packet = std::make_any<std::shared_ptr<MediaPacket>>(media_packet);
	BroadcastPacket(stream_packet);
}

void FileStream::SendAudioFrame(const std::shared_ptr<MediaPacket> &media_packet)
{
	if(GetState() != Stream::State::STARTED)
	{
		return;
	}
	
	auto stream_packet = std::make_any<std::shared_ptr<MediaPacket>>(media_packet);
	BroadcastPacket(stream_packet);
}

std::shared_ptr<FileSession> FileStream::CreateSession()
{
	auto session = FileSession::Create(GetApplication(), GetSharedPtrAs<pub::Stream>(), this->IssueUniqueSessionId());
	if (session == nullptr)
	{
		logte("Internal Error : Cannot create session");
		return nullptr;
	}

	logtd("created session");

	AddSession(session);

	return session;
}

bool FileStream::DeleteSession(uint32_t session_id)
{
	return RemoveSession(session_id);
}