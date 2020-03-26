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
protected:
	const char* GetApplicationTypeName() const override
	{
		return "RTMP Provider";
	}
public:
	static std::shared_ptr<RtmpApplication> Create(const info::Application &application_info);

	explicit RtmpApplication(const info::Application &info);
	~RtmpApplication() override = default;

	std::shared_ptr<pvd::Stream> CreateStream();

private:
};