//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

namespace cfg
{
	namespace mpegts
	{
		struct Stream : public Item
		{
		protected:
			ov::String _name{"stream"};
			RangedPort _port{"4000"};

		public:
			CFG_DECLARE_GETTER_OF(GetName, _name)
			CFG_DECLARE_GETTER_OF(GetPort, _port)

		protected:
			void MakeParseList() override
			{
				RegisterValue("Name", &_name);
				RegisterValue("Port", &_port);
			}
		};
	}  // namespace mpegts
}  // namespace cfg