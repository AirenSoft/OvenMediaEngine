//==============================================================================
//
//  RtmpProvider
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include "base/common_types.h"
#include "base/provider/stream.h"

using namespace pvd;

class RtmpStream : public Stream
{
public:
	static std::shared_ptr<RtmpStream> Create();

public:
	explicit RtmpStream();
	~RtmpStream() final;

private:

};