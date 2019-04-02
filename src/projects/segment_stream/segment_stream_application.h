//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once
#include "base/common_types.h"
#include "base/publisher/application.h"
#include "segment_stream.h"

//====================================================================================================
// SegmentStreamApplication
//====================================================================================================
class SegmentStreamApplication : public Application
{
public:
	static std::shared_ptr<SegmentStreamApplication> Create(const info::Application &application_info);
	SegmentStreamApplication(const info::Application &application_info);
	virtual ~SegmentStreamApplication() final;

private:
	bool Start() override;
	bool Stop() override;

	// Application Implementation
	std::shared_ptr<Stream> CreateStream(std::shared_ptr<StreamInfo> info, uint32_t worker_count) override;
	bool DeleteStream(std::shared_ptr<StreamInfo> info) override;

private :

};
