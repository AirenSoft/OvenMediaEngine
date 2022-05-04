//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2022 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/publisher/session.h>

class LLHlsSession : public pub::Session
{
public:
	static std::shared_ptr<LLHlsSession> Create(session_id_t session_id,
												const std::shared_ptr<pub::Application> &application,
												const std::shared_ptr<pub::Stream> &stream);

	LLHlsSession(const info::Session &session_info, 
				const std::shared_ptr<pub::Application> &application, 
				const std::shared_ptr<pub::Stream> &stream);

	bool Start() override;
	bool Stop() override;

	// pub::Session Interface
	void SendOutgoingData(const std::any &packet) override;
	void OnMessageReceived(const std::any &message) override;

private:

};