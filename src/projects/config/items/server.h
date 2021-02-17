//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "bind/bind.h"
#include "managers/managers.h"
#include "p2p/p2p.h"
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

		ov::String _typeName;
		ServerType _type;

		ov::String _ip;
		ov::String _stun_server;
		bind::Bind _bind;

		mgr::Managers _managers;

		p2p::P2P _p2p;

		vhost::VirtualHosts _virtual_hosts;

	public:
		CFG_DECLARE_REF_GETTER_OF(GetVersion, _version)

		CFG_DECLARE_REF_GETTER_OF(GetName, _name)

		CFG_DECLARE_REF_GETTER_OF(GetTypeName, _typeName)
		CFG_DECLARE_REF_GETTER_OF(GetType, _type)

		CFG_DECLARE_REF_GETTER_OF(GetIp, _ip)
		CFG_DECLARE_REF_GETTER_OF(GetStunServer, _stun_server)

		CFG_DECLARE_REF_GETTER_OF(GetBind, _bind)

		CFG_DECLARE_REF_GETTER_OF(GetManagers, _managers)

		CFG_DECLARE_REF_GETTER_OF(GetP2P, _p2p)

		CFG_DECLARE_REF_GETTER_OF(GetVirtualHostList, _virtual_hosts.GetVirtualHostList())

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

				return CreateConfigError("Unknown type: %s", _typeName.CStr());
			});

			Register({"IP", "ip"}, &_ip);
			Register<Optional>("StunServer", &_stun_server);
			Register("Bind", &_bind);

			Register<Optional>("Managers", &_managers);

			Register<Optional>({"P2P", "p2p"}, &_p2p);

			Register<Optional>("VirtualHosts", &_virtual_hosts);
		}
	};
}  // namespace cfg