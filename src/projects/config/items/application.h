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
#include "origin_listen.h"
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

		const OriginListen &GetRelay() const
		{
			return _relay;
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
			RegisterValue<Optional>("Relay", &_relay);
			RegisterValue<Optional>("Origin", &_origin);
			RegisterValue<Optional>("TLS", &_tls);
			RegisterValue<Optional>("Decode", &_decode);
			RegisterValue<Optional>("Encodes", &_encodes);
			RegisterValue<Optional>("Streams", &_streams);
			RegisterValue<Optional>("Providers", &_providers);
			RegisterValue<Optional>("Publishers", &_publishers);
		}

		ov::String _name;
		ov::String _type;
		OriginListen _relay;
		Origin _origin;
		Tls _tls;
		Decode _decode;
		Encodes _encodes;
		Streams _streams;
		Providers _providers;
		Publishers _publishers;
	};
}