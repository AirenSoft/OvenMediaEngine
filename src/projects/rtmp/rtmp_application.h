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

#include "base/provider/application.h"
#include "base/provider/stream.h"

using namespace pvd;

class RtmpApplication : public Application
{
public:
	static std::shared_ptr<RtmpApplication> Create(const info::Application *application_info);

	explicit RtmpApplication(const info::Application *info);
	~RtmpApplication() override = default;

public:
	std::shared_ptr<Stream> OnCreateStream() override;

private:
};