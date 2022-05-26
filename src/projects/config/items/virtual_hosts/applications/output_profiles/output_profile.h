//=============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "encodes/encodes.h"
#include "renditions.h"

namespace cfg
{
	namespace vhost
	{
		namespace app
		{
			namespace oprf
			{
				struct OutputProfile : public Item
				{
				protected:
					ov::String _name;
					ov::String _output_stream_name;
					Encodes _encodes;
					Renditions _renditions;

				public:
					CFG_DECLARE_CONST_REF_GETTER_OF(GetName, _name)
					CFG_DECLARE_CONST_REF_GETTER_OF(GetOutputStreamName, _output_stream_name)
					CFG_DECLARE_CONST_REF_GETTER_OF(GetEncodes, _encodes)
					CFG_DECLARE_CONST_REF_GETTER_OF(GetRenditions, _renditions)

				protected:
					void MakeList() override
					{
						Register("Name", &_name);
						Register("OutputStreamName", &_output_stream_name);
						Register<Optional>("Encodes", &_encodes);

						Register<Optional>("Renditions", &_renditions,
							[=]() -> std::shared_ptr<ConfigError> {
								return nullptr;
							},
							[=]() -> std::shared_ptr<ConfigError> {
								auto result = _renditions.SetEncodes(_encodes);

								return result ? nullptr : CreateConfigErrorPtr("Rendition Error");
							}
						);
					}
				};
			}  // namespace oprf
		}	   // namespace app
	}		   // namespace vhost
}  // namespace cfg
