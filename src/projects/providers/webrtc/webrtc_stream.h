//==============================================================================
//
//  WebRTC Provider
//
//  Created by Getroot
//  Copyright (c) 2021 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include "base/provider/stream.h"

namespace pvd
{
	class WebRTCStream : public pvd::Stream
	{
	public:
		static std::shared_ptr<WebRTCStream> Create(StreamSourceType source_type);
		
		explicit WebRTCStream(StreamSourceType source_type);
		~WebRTCStream() final;

		bool Start() override;
		bool Stop() override;
	};
}