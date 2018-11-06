//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "stream_profile.h"

namespace cfg
{
	struct StreamProfiles : public Item
	{
		bool MakeParseList() override
		{
			bool result = true;

			result = result && RegisterValue("Profile", &_stream_profile_list_temp);

			return result;
		}

		//template<typename Ttype>
		//bool UpcastVector(Value<std::vector<Ttype>> *from, std::vector<std::shared_ptr<Ttype>> *to)
		//{
		//	for(auto &value : from->GetValue())
		//	{
		//		auto value_as_type = std::dynamic_pointer_cast<Ttype>(value);
		//
		//		if(value_as_type == nullptr)
		//		{
		//			return false;
		//		}
		//
		//		to->push_back(value_as_type);
		//	}
		//
		//	return true;
		//}

		//const std::vector<std::shared_ptr<StreamProfile>> &GetProfiles() const
		//{
		//	return _stream_profile_list;
		//}

	protected:
		bool PostProcess(int indent)
		{
			//return UpcastVector(&_stream_profile_list_temp, &_stream_profile_list);
			return true;
		}

		Value<std::vector<StreamProfile>> _stream_profile_list_temp;
		//std::vector<std::shared_ptr<StreamProfile>> _stream_profile_list;
	};
}