//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/common_types.h>
#include <base/publisher/application.h>

class HlsPublisher;

class HlsApplication : public pub::Application
{
public:
	static std::shared_ptr<HlsApplication> Create(const std::shared_ptr<pub::Publisher> &publisher, const info::Application &application_info);

	HlsApplication(const std::shared_ptr<pub::Publisher> &publisher, const info::Application &application_info);
	~HlsApplication() override;

	//--------------------------------------------------------------------
	// Overriding of pub::Application
	//--------------------------------------------------------------------
	bool Start() override;

private:
	//--------------------------------------------------------------------
	// Overriding of pub::Application
	//--------------------------------------------------------------------
	std::shared_ptr<pub::Stream> CreateStream(const std::shared_ptr<info::Stream> &info, uint32_t thread_count) override;
	bool DeleteStream(const std::shared_ptr<info::Stream> &info) override;

protected:
	int _segment_count;
	int _segment_duration;
};
