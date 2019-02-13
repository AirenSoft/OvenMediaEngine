//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "publisher.h"
#include "signalling.h"
#include "tls.h"

namespace cfg
{
	struct WebrtcPublisher : public Publisher
	{
		PublisherType GetType() const override
		{
			return PublisherType::Webrtc;
		}

		ov::String GetPort() const
		{
			return _port;
		}

		const Signalling &GetSignalling() const
		{
			return _signalling;
		}

	protected:
		void MakeParseList() const override
		{
			Publisher::MakeParseList();

			RegisterValue<Optional>("Port", &_port);
			RegisterValue<Optional>("Timeout", &_timeout);
			RegisterValue("Signalling", &_signalling);
		}

		ov::String _port = "10000/udp";
		int _timeout = 0;
		Signalling _signalling;
	};
}