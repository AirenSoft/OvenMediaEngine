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
			protected:
				ov::String _name;
				ov::String _type;
				ApplicationType _type_value;

				dec::Decode _decode;
				oprf::OutputProfiles _output_profiles;
				pvd::Providers _providers;
				pub::Publishers _publishers;

			public:
				CFG_DECLARE_REF_GETTER_OF(GetName, _name)
				CFG_DECLARE_REF_GETTER_OF(GetType, _type_value)
				CFG_DECLARE_REF_GETTER_OF(GetTypeString, _type)

				CFG_DECLARE_REF_GETTER_OF(GetDecode, _decode)
				CFG_DECLARE_REF_GETTER_OF(GetOutputProfileList, _output_profiles.GetOutputProfileList())
				CFG_DECLARE_REF_GETTER_OF(GetOutputProfiles, _output_profiles)
				CFG_DECLARE_REF_GETTER_OF(GetProviders, _providers)
				CFG_DECLARE_REF_GETTER_OF(GetPublishers, _publishers)
				CFG_DECLARE_REF_GETTER_OF(GetStreamLoadBalancingThreadCount, _publishers.GetStreamLoadBalancingThreadCount())
				CFG_DECLARE_REF_GETTER_OF(GetSessionLoadBalancingThreadCount, _publishers.GetSessionLoadBalancingThreadCount())

			protected:
				void MakeList() override
				{
					Register("Name", &_name);
					Register("Type", &_type, nullptr, [=]() -> std::shared_ptr<ConfigError> {
						if (_type == "live")
						{
							_type_value = ApplicationType::Live;
							return nullptr;
						}
						else if (_type == "vod")
						{
							_type_value = ApplicationType::VoD;
							return nullptr;
						}

						return CreateConfigError("Unknown type: %s", _type.CStr());
					});

					Register<Optional>("Decode", &_decode);
					Register<Optional>("OutputProfiles", &_output_profiles);
					Register<Optional>("Providers", &_providers);
					Register<Optional>("Publishers", &_publishers);
				}
			};
		}  // namespace app
	}	   // namespace vhost
}  // namespace cfg