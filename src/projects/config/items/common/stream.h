//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2023 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

namespace cfg
{
	namespace cmn
	{
		struct Stream : public Item
		{
		protected:
			int _port;
			ov::String _url;

		public:
			CFG_DECLARE_CONST_REF_GETTER_OF(GetPort, _port)
			CFG_DECLARE_CONST_REF_GETTER_OF(GetUrl, _url)

		protected:
			void MakeList() override
			{
				Register("Port", &_port);
				Register("ListenURL", &_url);
			}
		};
	}  // namespace cmn
}  // namespace cfg
