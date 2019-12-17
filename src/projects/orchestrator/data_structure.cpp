//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#include "data_structure.h"

#define GET_TYPE_NAME(enum_type, value) \
	case enum_type::value:              \
		return #value

const char *GetOrchestratorModuleTypeName(OrchestratorModuleType type)
{
	switch (type)
	{
		GET_TYPE_NAME(OrchestratorModuleType, Unknown);
		GET_TYPE_NAME(OrchestratorModuleType, Provider);
		GET_TYPE_NAME(OrchestratorModuleType, MediaRouter);
		GET_TYPE_NAME(OrchestratorModuleType, Transcoder);
		GET_TYPE_NAME(OrchestratorModuleType, Publisher);
	}

	// Not handled type
	OV_ASSERT2(false);
	return "N/A";
}