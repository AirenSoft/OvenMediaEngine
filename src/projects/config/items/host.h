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
	enum class HostType
	{
		Unknown,
		Live,
		Vod,
		LiveEdge,
		VodEdge
	};

	struct Host : public Item
	{
		ov::String GetName() const
		{
			return _name;
		}

		HostType GetType() const
		{
			if(_type == "live")
			{
				return HostType::Live;
			}
			else if(_type == "liveedge")
			{
				return HostType::LiveEdge;
			}
			else if(_type == "vod")
			{
				return HostType::Vod;
			}
			else if(_type == "vodedge")
			{
				return HostType::VodEdge;
			}

			return HostType::Unknown;
		}

		ov::String GetTypeName() const
		{
			return _type;
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

		const WebConsole &GetWebConsole() const
		{
			return _web_console;
		}

		const std::vector<Application> &GetApplications() const
		{
			return _applications.GetApplications();
		}

	protected:
		void MakeParseList() const override
		{
			RegisterValue("Name", &_name);
			RegisterValue("Type", &_type);
			RegisterValue("IP", &_ip);
			RegisterValue<Optional>("TLS", &_tls);
			RegisterValue<Optional>("Ports", &_ports);
			RegisterValue<Optional>("Applications", &_applications);
			RegisterValue<Optional>("WebConsole", &_web_console);
		}
		
		ov::String _name;
		ov::String _type;
		ov::String _ip;
		Tls _tls;
		Ports _ports;
		WebConsole _web_console;
		Applications _applications;
	};
}
