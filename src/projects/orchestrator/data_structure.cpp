//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#include "data_structure.h"

#define GET_TYPE_NAME(enum_type, name, value)      \
	do                                             \
	{                                              \
		if (OV_CHECK_FLAG(value, enum_type::name)) \
		{                                          \
			list.push_back(#name);                 \
		}                                          \
	} while (false)

ov::String GetOrchestratorModuleTypeName(OrchestratorModuleType type)
{
	std::vector<ov::String> list;

	GET_TYPE_NAME(OrchestratorModuleType, Unknown, type);
	GET_TYPE_NAME(OrchestratorModuleType, Provider, type);
	GET_TYPE_NAME(OrchestratorModuleType, MediaRouter, type);
	GET_TYPE_NAME(OrchestratorModuleType, Transcoder, type);
	GET_TYPE_NAME(OrchestratorModuleType, Publisher, type);

	if (list.size() == 0)
	{
		// Unknown type name (not handled)
		OV_ASSERT2(false);
		return "N/A";
	}

	return ov::String::Join(list, " | ");
}