//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2022 AirenSoft. All rights reserved.
//
//==============================================================================
#include <modules/http/server/http_exchange.h>
#include "llhls_session.h"
#include "llhls_stream.h"
#include "llhls_private.h"

std::shared_ptr<LLHlsSession> LLHlsSession::Create(session_id_t session_id,
												const std::shared_ptr<pub::Application> &application,
												const std::shared_ptr<pub::Stream> &stream)
{
	auto session_info = info::Session(*std::static_pointer_cast<info::Stream>(stream), session_id);
	auto session = std::make_shared<LLHlsSession>(session_info, application, stream);

	if (session->Start() == false)
	{
		return nullptr;
	}

	return session;		
}

LLHlsSession::LLHlsSession(const info::Session &session_info, 
							const std::shared_ptr<pub::Application> &application, 
							const std::shared_ptr<pub::Stream> &stream)
	: pub::Session(session_info, application, stream)

{

}

bool LLHlsSession::Start()
{
	return Session::Start();
}

bool LLHlsSession::Stop()
{
	return Session::Stop();
}

// pub::Session Interface
void LLHlsSession::SendOutgoingData(const std::any &packet)
{

}

void LLHlsSession::OnMessageReceived(const std::any &message)
{
	std::shared_ptr<http::svr::HttpExchange> exchange = nullptr;
	try 
	{
        exchange = std::any_cast<std::shared_ptr<http::svr::HttpExchange>>(message);
		if(exchange == nullptr)
		{
			return;
		}
    }
    catch(const std::bad_any_cast& e) 
	{
        logtd("An incorrect type of packet was input from the stream.");
		return;
    }

	logti("LLHlsSession::OnMessageReceived(%u) - %s", GetId(), exchange->ToString().CStr());

	auto response = exchange->GetResponse();
	response->SetStatusCode(http::StatusCode::OK);
	response->AppendString("Test!!!");
	response->Response();
}
