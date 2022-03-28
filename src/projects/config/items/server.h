//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <fstream>

#include "analytics/analytics.h"
#include "base/ovlibrary/uuid.h"
#include "bind/bind.h"
#include "modules/modules.h"
#include "managers/managers.h"
#include "virtual_hosts/virtual_hosts.h"

namespace cfg
{
	enum class ServerType
	{
		Unknown,
		Origin,
		Edge
	};

	struct Server : public Item
	{
	protected:
		Attribute _version;

		ov::String _name;
		ov::String _id;

		bool _privacy_protection_on = false;

		ov::String _typeName;
		ServerType _type;

		ov::String _ip;
		ov::String _stun_server;
		bind::Bind _bind;
		modules::modules _modules;

		mgr::Managers _managers;

		an::Analytics _analytics;

		vhost::VirtualHosts _virtual_hosts;

	public:
		CFG_DECLARE_CONST_REF_GETTER_OF(GetVersion, _version)

		CFG_DECLARE_CONST_REF_GETTER_OF(GetName, _name)

		CFG_DECLARE_CONST_REF_GETTER_OF(GetTypeName, _typeName)
		CFG_DECLARE_CONST_REF_GETTER_OF(GetType, _type)

		CFG_DECLARE_CONST_REF_GETTER_OF(GetIp, _ip)
		CFG_DECLARE_CONST_REF_GETTER_OF(GetStunServer, _stun_server)

		CFG_DECLARE_CONST_REF_GETTER_OF(IsPrivaryProtectionOn, _privacy_protection_on)

		CFG_DECLARE_CONST_REF_GETTER_OF(GetBind, _bind)

		CFG_DECLARE_CONST_REF_GETTER_OF(GetModules, _modules)

		CFG_DECLARE_CONST_REF_GETTER_OF(GetManagers, _managers)

		CFG_DECLARE_CONST_REF_GETTER_OF(GetAnalytics, _analytics)

		CFG_DECLARE_CONST_REF_GETTER_OF(GetVirtualHostList, _virtual_hosts.GetVirtualHostList())

		ov::String GetID() const
		{
			return _id;
		}
		// Set ID from external config file
		void SetID(ov::String id)
		{
			_id = id;
		}

		// Deprecated - It has a bug
		bool GetVirtualHostByName(ov::String name, cfg::vhost::VirtualHost &vhost) const
		{
			auto &vhost_list = GetVirtualHostList();
			for (auto &item : vhost_list)
			{
				if (item.GetName() == name)
				{
					vhost = item;
					return true;
				}
			}

			return false;
		}

	protected:
		void MakeList() override
		{
			Register("version", &_version);

			Register<Optional>("Name", &_name);

			Register("Type", &_typeName, nullptr, [=]() -> std::shared_ptr<ConfigError> {
				_type = ServerType::Unknown;

				if (_typeName == "origin")
				{
					_type = ServerType::Origin;
					return nullptr;
				}
				else if (_typeName == "edge")
				{
					_type = ServerType::Edge;
					return nullptr;
				}

				return CreateConfigErrorPtr("Unknown type: %s", _typeName.CStr());
			});

			Register({"IP", "ip"}, &_ip);
			Register<Optional>("StunServer", &_stun_server);
			Register<Optional>("PrivacyProtection", &_privacy_protection_on);
			Register("Bind", &_bind);
			Register<Optional>("Modules", &_modules);

			Register<Optional>("Managers", &_managers);
			Register<Optional>("Analytics", &_analytics);

			Register<Optional>("VirtualHosts", &_virtual_hosts);
		}
	};
}  // namespace cfg