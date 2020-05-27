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
#include "base/provider/push_provider/application.h"
#include "base/provider/push_provider/stream.h"

class RtmpApplication : public pvd::PushApplication
{
protected:
	std::shared_ptr<pvd::PushStream> CreateStream(const uint32_t stream_id, const ov::String &stream_name) override;

public:
	static std::shared_ptr<RtmpApplication> Create(const std::shared_ptr<pvd::PushProvider> &provider, const info::Application &application_info);

	explicit RtmpApplication(const std::shared_ptr<pvd::PushProvider> &provider, const info::Application &info);
	~RtmpApplication() override = default;

private:
};