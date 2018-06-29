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

#include "base/application/application.h"

#include "base/provider/application.h"
#include "base/provider/stream.h"

using namespace pvd;

class RtmpApplication : public Application
{
public:
	static std::shared_ptr<RtmpApplication> Create(const ApplicationInfo &info);

	RtmpApplication(const ApplicationInfo &info);
	~RtmpApplication() final;

public:
	std::shared_ptr<Stream> OnCreateStream() override;

private:
};