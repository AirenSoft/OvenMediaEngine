//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "application.h"

namespace cfg
{
	struct IceCandidate : public Item
	{
		ov::String GetCandidate() const
		{
			return _candidate;
		}

	protected:
		void MakeParseList() const override
		{
			RegisterValue<ValueType::Text, Optional>(nullptr, &_candidate);
		}

		ov::String _candidate;
	};
}