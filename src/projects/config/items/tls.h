//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "../item.h"

namespace cfg
{
	struct Tls : public Item
	{
		bool MakeParseList() override
		{
			bool result = true;

			result = result && RegisterValue("CertPath", &_cert_path);
			result = result && RegisterValue("KeyPath", &_key_path);

			return result;
		}

		ov::String GetCertPath() const
		{
			return *_cert_path;
		}

		ov::String GetKeyPath() const
		{
			return *_key_path;
		}

	protected:
		Value<ov::String> _cert_path;
		Value<ov::String> _key_path;
	};
}