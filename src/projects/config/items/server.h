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
		CFG_DECLARE_REF_GETTER_OF(GetVersion, _version)

		CFG_DECLARE_REF_GETTER_OF(GetName, _name)

		CFG_DECLARE_REF_GETTER_OF(GetTypeName, _typeName)
		CFG_DECLARE_GETTER_OF(GetType, _type)

		CFG_DECLARE_REF_GETTER_OF(GetIp, _ip)
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
		void MakeParseList() override
		{
			RegisterValue<ValueType::Attribute>("version", &_version);

			RegisterValue<Optional>("Name", &_name);

			RegisterValue("Type", &_typeName, nullptr, [this]() -> bool {
				_type = ServerType::Unknown;

				if (_typeName == "origin")
				{
					_type = ServerType::Origin;
				}
				else if (_typeName == "edge")
				{
					_type = ServerType::Edge;
				}

				return _type != ServerType::Unknown;
			});

			RegisterValue("IP", &_ip);
			RegisterValue("Bind", &_bind);

			RegisterValue<Optional>("Managers", &_managers);

			RegisterValue<Optional>("P2P", &_p2p);

			RegisterValue<Optional>("VirtualHosts", &_virtual_hosts);
		}

		ov::String _version;

		ov::String _name;

		ov::String _typeName;
		ServerType _type;

		ov::String _ip;
		bind::Bind _bind;

		mgr::Managers _managers;

		p2p::P2P _p2p;

		vhost::VirtualHosts _virtual_hosts;
	};
}  // namespace cfg