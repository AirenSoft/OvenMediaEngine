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

		const Providers &GetProviders() const
		{
			return _providers;
		}

		const Publishers &GetPublishers() const
		{
			return _publishers;
		}

		const std::vector<Application> &GetApplications() const
		{
			return _applications.GetApplications();
		}

        int GetMonitoringPort() const
        {
            return _monitoring_port;
        }

	protected:
		void MakeParseList() const override
		{
			RegisterValue("Name", &_name);
			RegisterValue<Optional>("IP", &_ip);
			RegisterValue<Optional>("TLS", &_tls);
			RegisterValue<Optional>("Ports", &_ports);
			RegisterValue<Optional>("Providers", &_providers);
			RegisterValue<Optional>("Publishers", &_publishers);
			RegisterValue<Optional>("Applications", &_applications);
			RegisterValue<Optional>("MonitoringPort", &_monitoring_port);
		}
		
		ov::String _name;
		ov::String _ip;
		Tls _tls;
		Ports _ports;
		Providers _providers;
		Publishers _publishers;
		Applications _applications;
		int _monitoring_port = 8888;
	};
}
