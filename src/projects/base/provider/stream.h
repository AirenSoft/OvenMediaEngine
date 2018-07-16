//==============================================================================
//
//  Provider Base Class 
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "base/common_types.h"
#include "base/application/stream_info.h"

namespace pvd
{
	class Stream : public StreamInfo
	{
	public:

	protected:
		Stream();
		virtual ~Stream();

	};
}