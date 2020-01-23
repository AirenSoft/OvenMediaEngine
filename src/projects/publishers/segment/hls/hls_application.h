//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once
#include "base/common_types.h"
#include "base/publisher/application.h"
#include <publishers/segment/segment_stream/segment_stream.h>

//====================================================================================================
// HlsApplication
//====================================================================================================
class HlsApplication : public Application
{
public:
	static std::shared_ptr<HlsApplication> Create(const info::Application &application_info);

    HlsApplication(const info::Application &application_info);

	virtual ~HlsApplication() final;

private:
	bool Start() override;
	bool Stop() override;

	// Application Implementation
	std::shared_ptr<Stream> CreateStream(const std::shared_ptr<StreamInfo> &info, uint32_t thread_count) override;
	bool DeleteStream(const std::shared_ptr<StreamInfo> &info) override;

private :
    int _segment_count;
    int _segment_duration;
};
