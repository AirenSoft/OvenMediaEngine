//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "origin.h"
#include "web_console.h"
#include "decode.h"
#include "encodes.h"
#include "streams.h"
#include "providers.h"
#include "publishers.h"

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
			else if(_type == "liveedge")
			{
				return ApplicationType::LiveEdge;
			}
			else if(_type == "vod")
			{
				return ApplicationType::Vod;
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

		const Origin &GetOrigin() const
		{
			return _origin;
		}

		const WebConsole &GetWebConsole() const
		{
			return _web_console;
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

		const Providers &GetProviders() const
		{
			return _providers;
		}

		const Publishers &GetPublishers() const
		{
			return _publishers;
		}

		const int GetThreadCount() const
		{
			return _publishers.GetThreadCount();
		}

	protected:
		void MakeParseList() const override
		{
			RegisterValue("Name", &_name);
			RegisterValue("Type", &_type);
			RegisterValue<Optional>("Origin", &_origin);
			RegisterValue<Optional>("WebConsole", &_web_console);
			RegisterValue<Optional>("Decode", &_decode);
			RegisterValue<Optional>("Encodes", &_encodes);
			RegisterValue<Optional>("Streams", &_streams);
			RegisterValue<Optional>("Providers", &_providers);
			RegisterValue<Optional>("Publishers", &_publishers);
		}

		ov::String _name;
		ov::String _type;
		Origin _origin;
		WebConsole _web_console;
		Decode _decode;
		Encodes _encodes;
		Streams _streams;
		Providers _providers;
		Publishers _publishers;
	};
}