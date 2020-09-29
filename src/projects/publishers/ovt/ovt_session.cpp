#include <base/info/stream.h>
#include <base/ovlibrary/byte_io.h>
#include <base/publisher/stream.h>
#include <modules/ovt_packetizer/ovt_packet.h>
#include "ovt_session.h"
#include "ovt_private.h"

std::shared_ptr<OvtSession> OvtSession::Create(const std::shared_ptr<pub::Application> &application,
										  	   const std::shared_ptr<pub::Stream> &stream,
										  	   uint32_t session_id,
										  	   const std::shared_ptr<ov::Socket> &connector)
{
	auto session_info = info::Session(*std::static_pointer_cast<info::Stream>(stream), session_id);
	auto session = std::make_shared<OvtSession>(session_info, application, stream, connector);
	if(!session->Start())
	{
		return nullptr;
	}
	return session;
}

OvtSession::OvtSession(const info::Session &session_info,
		   const std::shared_ptr<pub::Application> &application,
		   const std::shared_ptr<pub::Stream> &stream,
		   const std::shared_ptr<ov::Socket> &connector)
   : pub::Session(session_info, application, stream)
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
	_connector->Close();
	
	return Session::Stop();
}

bool OvtSession::SendOutgoingData(const std::any &packet)
{
	std::shared_ptr<OvtPacket> session_packet;

	try 
	{
        session_packet = std::any_cast<std::shared_ptr<OvtPacket>>(packet);
		if(session_packet == nullptr)
		{
			return false;
		}
    }
    catch(const std::bad_any_cast& e) 
	{
        logtd("An incorrect type of packet was input from the stream. (%s)", e.what());
		return false;
    }

	// OvtSession should send full packet so it will start to send from next packet of marker packet.
	if(_sent_ready == false)
	{
		if(session_packet->Marker() == true) // Set marker
		{
			_sent_ready = true;
		}

		return false;
	}

	// Set OVT Session ID into packet
	auto copy_packet = std::make_shared<OvtPacket>(*session_packet);
	copy_packet->SetSessionId(GetId());
	_connector->Send(copy_packet->GetData());

	return true;
}

const std::shared_ptr<ov::Socket> OvtSession::GetConnector()
{
	return _connector;
}

void OvtSession::OnPacketReceived(const std::shared_ptr<info::Session> &session_info,
									const std::shared_ptr<const ov::Data> &data)
{
	// NOTHING YET
}