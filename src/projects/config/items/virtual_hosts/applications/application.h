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
#include "origin.h"
#include "output_profiles/output_profiles.h"
#include "providers/providers.h"
#include "publishers/publishers.h"
#include "web_console/web_console.h"

namespace cfg
{
	namespace vhost
	{
		namespace app
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
				CFG_DECLARE_REF_GETTER_OF(GetTypeString, _type)

				CFG_DECLARE_REF_GETTER_OF(GetDecode, _decode)
				CFG_DECLARE_REF_GETTER_OF(GetOutputProfileList, _output_profiles.GetOutputProfileList())
				CFG_DECLARE_REF_GETTER_OF(GetOutputProfiles, _output_profiles)
				CFG_DECLARE_REF_GETTER_OF(GetProviders, _providers)
				CFG_DECLARE_REF_GETTER_OF(GetPublishers, _publishers)
				CFG_DECLARE_GETTER_OF(GetStreamLoadBalancingThreadCount, _publishers.GetStreamLoadBalancingThreadCount())
				CFG_DECLARE_GETTER_OF(GetSessionLoadBalancingThreadCount, _publishers.GetSessionLoadBalancingThreadCount())

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

					RegisterValue<Optional>("Decode", &_decode);
					RegisterValue<Optional>("OutputProfiles", &_output_profiles);
					RegisterValue<Optional>("Providers", &_providers);
					RegisterValue<Optional>("Publishers", &_publishers);
				}

				ov::String _name;
				ov::String _type;
				ApplicationType _type_value;

				dec::Decode _decode;
				oprf::OutputProfiles _output_profiles;
				pvd::Providers _providers;
				pub::Publishers _publishers;
			};
		}  // namespace app
	}	   // namespace vhost
}  // namespace cfg