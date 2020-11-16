//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

namespace cfg
{
	namespace vhost
	{
		namespace app
		{
			struct Origin : public Item
			{
				CFG_DECLARE_REF_GETTER_OF(GetPrimary, _primary)
				CFG_DECLARE_REF_GETTER_OF(GetSecondary, _secondary)
				CFG_DECLARE_REF_GETTER_OF(GetAlias, _alias)

			protected:
				void MakeParseList() override
				{
					RegisterValue<Optional>("Primary", &_primary);
					RegisterValue<Optional>("Secondary", &_secondary);
					RegisterValue<Optional>("Alias", &_alias);
				}

				ov::String _primary;
				ov::String _secondary;
				ov::String _alias;
			};
		}  // namespace app
	}	   // namespace vhost
}  // namespace cfg