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
#include "base/info/stream_info.h"

namespace pvd
{
	class Stream : public StreamInfo, public ov::EnableSharedFromThis<Stream>
	{
	public:
		enum class State
		{
			IDLE,
			CONNECTED,
			DESCRIBED,
			PLAYING,
			STOPPED,
			ERROR
		};

		State GetState(){return _state;};

	protected:
		Stream();
		Stream(uint32_t stream_id);
		Stream(const StreamInfo &stream_info);

		virtual ~Stream();

		State 	_state;
	};
}