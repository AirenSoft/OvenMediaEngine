#include <base/ovlibrary/byte_io.h>
#include "ovt_session.h"
#include "ovt_private.h"

std::shared_ptr<OvtSession> OvtSession::Create(const std::shared_ptr<Application> &application,
										  const std::shared_ptr<Stream> &stream,
										  uint32_t session_id,
										  const std::shared_ptr<ov::Socket> &connector)
{

	auto session_info = SessionInfo(session_id);
	auto session = std::make_shared<OvtSession>(session_info, application, stream, connector);
	if(!session->Start())
	{
		return nullptr;
	}
	return session;
}

OvtSession::OvtSession(const SessionInfo &session_info,
		   const std::shared_ptr<Application> &application,
		   const std::shared_ptr<Stream> &stream,
		   const std::shared_ptr<ov::Socket> &connector)
   : Session(session_info, application, stream)
{
	_connector = connector;
	_sent_ready = false;
}

OvtSession::~OvtSession()
{
	Stop();
	logtd("OvtSession(%d) has been terminated finally", GetId());
}

bool OvtSession::Start()
{
	logtd("OvtSession(%d) has started", GetId());
	return Session::Start();
}

bool OvtSession::Stop()
{
	logtd("OvtSession(%d) has stopped", GetId());
	return Session::Stop();
}

bool OvtSession::SendOutgoingData(uint32_t packet_type, const std::shared_ptr<ov::Data> &packet)
{
	// packet_type in OvtSession means marker of OVT Packet
	// OvtSession should send full packet so it will start to send from next packet of marker packet.
	if(_sent_ready == false)
	{
		if(packet_type == true) // Set marker
		{
			_sent_ready = true;
		}

		return false;
	}

	// Set OVT Session ID into packet
	// It is also possible to use OvtPacket::Load, but for performance, as follows.
	auto buffer = packet->GetWritableDataAs<uint8_t>();
	ByteWriter<uint16_t>::WriteBigEndian(&buffer[12], GetId());

	_connector->Send(packet->GetData(), packet->GetLength());
}

const std::shared_ptr<ov::Socket> OvtSession::GetConnector()
{
	return _connector;
}

void OvtSession::OnPacketReceived(const std::shared_ptr<SessionInfo> &session_info,
									const std::shared_ptr<const ov::Data> &data)
{
	// NOTHING YET
}