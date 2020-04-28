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
	std::shared_ptr<pvd::Stream> CreatePushStream(const uint32_t stream_id, const ov::String &stream_name) override;
	std::shared_ptr<pvd::Stream> CreatePullStream(const uint32_t stream_id, const ov::String &stream_name, const std::vector<ov::String> &url_list) override {return nullptr;};

public:
	static std::shared_ptr<RtmpApplication> Create(const std::shared_ptr<Provider> &provider, const info::Application &application_info);

	explicit RtmpApplication(const std::shared_ptr<Provider> &provider, const info::Application &info);
	~RtmpApplication() override = default;

private:
};