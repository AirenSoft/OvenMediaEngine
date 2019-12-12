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
	struct IceCandidate : public Item
	{
		IceCandidate() = default;
		explicit IceCandidate(const char *candidate)
			: _candidate(candidate)
		{
		}

		CFG_DECLARE_REF_GETTER_OF(GetCandidate, _candidate);

	protected:
		void MakeParseList() override
		{
			RegisterValue<ValueType::Text>(nullptr, &_candidate);
		}

		ov::String _candidate;
	};
}  // namespace cfg