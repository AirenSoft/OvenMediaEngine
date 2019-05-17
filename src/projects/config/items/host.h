//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "tls.h"
#include "ports.h"
#include "providers.h"
#include "publishers.h"
#include "applications.h"

namespace cfg
{
	struct Host : public Item
	{
		ov::String GetName() const
		{
			return _name;
		}

		ov::String GetIp() const
		{
			return _ip;
		}

		const Tls &GetTls() const
		{
			return _tls;
		}

		const Ports &GetPorts() const
		{
			return _ports;
		}

		const std::vector<Application> &GetApplications() const
		{
			return _applications.GetApplications();
		}

	protected:
		void MakeParseList() const override
		{
			RegisterValue("Name", &_name);
			RegisterValue("IP", &_ip);
			RegisterValue<Optional>("TLS", &_tls);
			RegisterValue<Optional>("Ports", &_ports);
			RegisterValue<Optional>("Applications", &_applications);
		}
		
		ov::String _name;
		ov::String _ip;
		Tls _tls;
		Ports _ports;
		Applications _applications;
	};
}
