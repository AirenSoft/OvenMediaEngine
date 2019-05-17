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
		IceCandidate() = default;
		explicit IceCandidate(const char *candidate)
			: _candidate(candidate)
		{
		}

		ov::String GetCandidate() const
		{
			return _candidate;
		}

	protected:
		void MakeParseList() const override
		{
			RegisterValue<ValueType::Text>(nullptr, &_candidate);
		}

		ov::String _candidate;
	};
}