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
#include "decode.h"
#include "encodes.h"
#include "streams.h"
#include "providers.h"
#include "publishers.h"
#include "origin.h"

namespace cfg
{
	enum class ApplicationType
	{
		Unknown,
		Live,
		Vod,
		LiveEdge,
		VodEdge
	};

	struct Application : public Item
	{
		ov::String GetName() const
		{
			return _name;
		}

		ApplicationType GetType() const
		{
			if(_type == "live")
			{
				return ApplicationType::Live;
			}
			else if(_type == "vod")
			{
				return ApplicationType::Vod;
			}
			else if(_type == "liveedge")
			{
				return ApplicationType::LiveEdge;
			}
			else if(_type == "vodedge")
			{
				return ApplicationType::VodEdge;
			}

			return ApplicationType::Unknown;
		}

		ov::String GetTypeName() const
		{
			return _type;
		}

		int GetRelayPort() const
		{
			return _relay_port;
		}

		const Origin &GetOrigin() const
		{
			return _origin;
		}

		const Tls &GetTls() const
		{
			return _tls;
		}

		const Decode &GetDecode() const
		{
			return _decode;
		}

		const std::vector<Encode> &GetEncodes() const
		{
			return _encodes.GetEncodes();
		}

		const std::vector<Stream> &GetStreamList() const
		{
			return _streams.GetStreamList();
		}

		std::vector<const Provider *> GetProviders() const
		{
			return std::move(_providers.GetProviders());
		}

		std::vector<const Publisher *> GetPublishers() const
		{
			return std::move(_publishers.GetPublishers());
		}

	protected:
		void MakeParseList() const override
		{
			RegisterValue("Name", &_name);
			RegisterValue<Optional>("Type", &_type);
			RegisterValue<Optional>("RelayPort", &_relay_port);
			RegisterValue<Optional, Includable>("Origin", &_origin);
			RegisterValue<Optional, Includable>("TLS", &_tls);
			RegisterValue<Optional, Includable>("Decode", &_decode);
			RegisterValue<Optional, Includable>("Encodes", &_encodes);
			RegisterValue<Optional, Includable>("Streams", &_streams);
			RegisterValue<Optional, Includable>("Providers", &_providers);
			RegisterValue<Optional, Includable>("Publishers", &_publishers);
		}

		ov::String _name;
		ov::String _type;
		int _relay_port = 9000;
		Origin _origin;
		Tls _tls;
		Decode _decode;
		Encodes _encodes;
		Streams _streams;
		Providers _providers;
		Publishers _publishers;
	};
}