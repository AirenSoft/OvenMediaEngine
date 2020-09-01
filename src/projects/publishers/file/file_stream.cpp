

#include "file_private.h"
#include "file_stream.h"
#include "file_session.h"
#include "base/publisher/application.h"
#include "base/publisher/stream.h"

std::shared_ptr<FileStream> FileStream::Create(const std::shared_ptr<pub::Application> application,
											 const info::Stream &info,
											 uint32_t worker_count)
{
	auto stream = std::make_shared<FileStream>(application, info);
	if(!stream->Start(worker_count))
	{
		return nullptr;
	}
	return stream;
}

FileStream::FileStream(const std::shared_ptr<pub::Application> application,
					 const info::Stream &info)
		: Stream(application, info)
{
}

FileStream::~FileStream()
{
	logtd("FileStream(%s/%s) has been terminated finally", GetApplicationName() , GetName().CStr());
}

bool FileStream::Start(uint32_t worker_count)
{
	logtd("FileStream(%ld) has been started", GetId());
	_packetizer = std::make_shared<OvtPacketizer>(OvtPacketizerInterface::GetSharedPtr());



	_stream_metrics = StreamMetrics(*std::static_pointer_cast<info::Stream>(pub::Stream::GetSharedPtr()));

	return Stream::Start(worker_count);
}

bool FileStream::Stop()
{
	logtd("FileStream(%u) has been stopped", GetId());

	std::unique_lock<std::mutex> mlock(_packetizer_lock);
	if(_packetizer != nullptr)
	{	
		_packetizer.reset();
		_packetizer = nullptr;
	}

	return Stream::Stop();
}

void FileStream::SendVideoFrame(const std::shared_ptr<MediaPacket> &media_packet)
{
//	logte("-");
	
	// Callback OnOvtPacketized()
	std::unique_lock<std::mutex> mlock(_packetizer_lock);
	if(_packetizer != nullptr)
	{
		_packetizer->Packetize(media_packet->GetPts(), media_packet);
	}
}

void FileStream::SendAudioFrame(const std::shared_ptr<MediaPacket> &media_packet)
{
//	logte("=");

	// Callback OnOvtPacketized()
	std::unique_lock<std::mutex> mlock(_packetizer_lock);
	if(_packetizer != nullptr)
	{
		_packetizer->Packetize(media_packet->GetPts(), media_packet);
	}
}

bool FileStream::OnOvtPacketized(std::shared_ptr<OvtPacket> &packet)
{
	// Broadcasting
	BroadcastPacket(packet->Marker(), packet->GetData());
	if(_stream_metrics != nullptr)
	{
		_stream_metrics->IncreaseBytesOut(PublisherType::Ovt, packet->GetData()->GetLength() * GetSessionCount());
	}

	return true;
}

bool FileStream::RemoveSessionByConnectorId(int connector_id)
{
	auto sessions = GetAllSessions();

	logtd("RemoveSessionByConnectorId : all(%d) connector(%d)", sessions.size(), connector_id);

	for(const auto &item : sessions)
	{
		auto session = std::static_pointer_cast<FileSession>(item.second);
		logtd("session : %d %d", session->GetId(), session->GetConnector()->GetId());

		if(session->GetConnector()->GetId() == connector_id)
		{
			RemoveSession(session->GetId());
			return true;
		}
	}

	return false;
}