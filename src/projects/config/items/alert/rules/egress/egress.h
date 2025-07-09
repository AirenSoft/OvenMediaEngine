//=============================================================================
//
//  OvenMediaEngine
//
//  Created by Gilhoon Choi
//  Copyright (c) 2025 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

namespace cfg
{
	namespace alrt
	{
		namespace rule
		{
			struct Egress : public Item
			{
			protected:
				bool _stream_status = false;

			public:
				CFG_DECLARE_CONST_REF_GETTER_OF(IsStreamStatus, _stream_status)

			protected:
				void MakeList() override
				{
					Register<Optional>("StreamStatus", &_stream_status, nullptr, [=]() -> std::shared_ptr<ConfigError> {
						_stream_status = true;
						return nullptr;
					});
				}
			};
		}  // namespace rule
	}  // namespace alrt
}  // namespace cfg
