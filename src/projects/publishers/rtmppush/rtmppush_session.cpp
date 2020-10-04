#include <base/info/stream.h>
#include <base/ovlibrary/byte_io.h>
#include <base/publisher/stream.h>
#include "rtmppush_session.h"
#include "rtmppush_private.h"

std::shared_ptr<RtmpPushSession> RtmpPushSession::Create(const std::shared_ptr<pub::Application> &application,
										  	   const std::shared_ptr<pub::Stream> &stream,
										  	   uint32_t session_id,
										  	   const std::shared_ptr<ov::Socket> &connector)
{
	auto session_info = info::Session(*std::static_pointer_cast<info::Stream>(stream), session_id);
	auto session = std::make_shared<RtmpPushSession>(session_info, application, stream, connector);
	if(!session->Start())
	{
		return nullptr;
	}
	return session;
}

RtmpPushSession::RtmpPushSession(const info::Session &session_info,
		   const std::shared_ptr<pub::Application> &application,
		   const std::shared_ptr<pub::Stream> &stream,
		   const std::shared_ptr<ov::Socket> &connector)
   : pub::Session(session_info, application, stream)
{
	_connector = connector;
	_sent_ready = false;
}

RtmpPushSession::~RtmpPushSession()
{
	Stop();
	logtd("RtmpPushSession(%d) has been terminated finally", GetId());
}

bool RtmpPushSession::Start()
{
	logtd("RtmpPushSession(%d) has started", GetId());
	return Session::Start();
}

bool RtmpPushSession::Stop()
{
	logtd("RtmpPushSession(%d) has stopped", GetId());
	_connector->Close();
	
	return Session::Stop();
}

bool RtmpPushSession::SendOutgoingData(const std::any &packet)
{

	return true;
}

const std::shared_ptr<ov::Socket> RtmpPushSession::GetConnector()
{
	return _connector;
}

void RtmpPushSession::OnPacketReceived(const std::shared_ptr<info::Session> &session_info,
									const std::shared_ptr<const ov::Data> &data)
{
	
}