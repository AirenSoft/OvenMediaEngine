//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2021 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/ovlibrary/ovlibrary.h>

#include "config_error.h"
#include "item_name.h"
#include "value_type.h"

namespace cfg
{
	class Item;

	class ListChild
	{
	public:
		ListChild(
			const ItemName &item_name,
			std::any &target,
			const Json::Value &original_value)
			: _item_name(item_name),
			  _target(target),
			  _original_value(original_value)
		{
		}

		const ItemName &GetItemName() const
		{
			return _item_name;
		}

		std::any &GetTarget()
		{
			return _target;
		}

		const Json::Value &GetOriginalValue() const
		{
			return _original_value;
		}

		void Update(std::any target)
		{
			_target = target;
		}

	protected:
		ItemName _item_name;
		std::any _target;
		Json::Value _original_value;
	};
}  // namespace cfg
