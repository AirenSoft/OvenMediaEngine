//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

namespace cfg
{
	struct SignedPolicyEnabledProviders : public Item
	{
		CFG_DECLARE_REF_GETTER_OF(GetValue, _value)
		CFG_DECLARE_REF_GETTER_OF(GetValueList, _value_list)
		bool IsExist(ov::String value) const
		{
			for(const auto &item : _value_list)
			{
				if(item.UpperCaseString() == value.UpperCaseString())
				{
					return true;
				}
			}

			return false;
		}

	protected:
		void MakeParseList() override
		{
			RegisterValue<ValueType::Text>(nullptr, &_value, nullptr, [this]() -> bool {
			_value_list = _value.Trim().Split(",");
			return true;
			});

		}

		ov::String _value;
		std::vector<ov::String>	_value_list;
	};
}  // namespace cfg