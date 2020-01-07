//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "decode/decode.h"
#include "encodes/encodes.h"
#include "origin.h"
#include "providers/providers.h"
#include "publishers/publishers.h"
#include "streams/streams.h"
#include "web_console/web_console.h"

namespace cfg
{
	enum class ApplicationType
	{
		Unknown,
		Live,
		VoD
	};

	struct Application : public Item
	{
		CFG_DECLARE_REF_GETTER_OF(GetName, _name)
		CFG_DECLARE_REF_GETTER_OF(GetType, _type_value)

		CFG_DECLARE_REF_GETTER_OF(GetOrigin, _origin)
		CFG_DECLARE_REF_GETTER_OF(GetDecode, _decode)
		CFG_DECLARE_REF_GETTER_OF(GetEncodeList, _encodes.GetEncodeList())
		CFG_DECLARE_REF_GETTER_OF(GetStreamList, _streams.GetStreamList())
		CFG_DECLARE_REF_GETTER_OF(GetProviders, _providers)
		CFG_DECLARE_REF_GETTER_OF(GetPublishers, _publishers)
		CFG_DECLARE_GETTER_OF(GetThreadCount, _publishers.GetThreadCount())

	protected:
		void MakeParseList() override
		{
			RegisterValue("Name", &_name);
			RegisterValue("Type", &_type, nullptr, [this]() -> bool {
				if (_type == "live")
				{
					_type_value = ApplicationType::Live;
				}
				else if (_type == "vod")
				{
					_type_value = ApplicationType::VoD;
				}
				else
				{
					return false;
				}

				return true;
			});

			RegisterValue<Optional>("Origin", &_origin);
			RegisterValue<Optional>("Decode", &_decode);
			RegisterValue<Optional>("Encodes", &_encodes);
			RegisterValue<Optional>("Streams", &_streams);
			RegisterValue<Optional>("Providers", &_providers);
			RegisterValue<Optional>("Publishers", &_publishers);
		}

		ov::String _name;
		ov::String _type;
		ApplicationType _type_value;

		Origin _origin;
		Decode _decode;
		Encodes _encodes;
		Streams _streams;
		Providers _providers;
		Publishers _publishers;
	};
}  // namespace cfg