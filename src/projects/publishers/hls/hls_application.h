//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2022 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/common_types.h>
#include <base/info/session.h>
#include <base/publisher/application.h>
#include <modules/http/http.h>

#include "hls_stream.h"

class HlsApplication : public pub::Application
{
public:
	static std::shared_ptr<HlsApplication> Create(const std::shared_ptr<pub::Publisher> &publisher, const info::Application &application_info);
	HlsApplication(const std::shared_ptr<pub::Publisher> &publisher, const info::Application &application_info);
	~HlsApplication() final;

	const http::CorsManager &GetCorsManager() const
	{
		return _cors_manager;
	}

	bool IsOriginMode() const
	{
		return _origin_mode;
	}

private:
	bool Start() override;
	bool Stop() override;

	// Application Implementation
	std::shared_ptr<pub::Stream> CreateStream(const std::shared_ptr<info::Stream> &info, uint32_t worker_count) override;
	bool DeleteStream(const std::shared_ptr<info::Stream> &info) override;

	http::CorsManager _cors_manager;
	bool _origin_mode = false;
};
