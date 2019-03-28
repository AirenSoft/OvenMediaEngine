//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "./origin_listen.h"

namespace cfg
{
	struct Origin : public Item
	{
		const OriginListen &GetListen() const
		{
			return _listen;
		}

		ov::String GetPrimary() const
		{
			return _primary;
		}

		ov::String GetSecondary() const
		{
			return _secondary;
		}

	protected:
		void MakeParseList() const override
		{
			RegisterValue<Optional>("Listen", &_listen);
			RegisterValue<Optional>("Primary", &_primary);
			RegisterValue<Optional>("Secondary", &_secondary);
		}

		OriginListen _listen;

		ov::String _primary;
		ov::String _secondary;
	};
}