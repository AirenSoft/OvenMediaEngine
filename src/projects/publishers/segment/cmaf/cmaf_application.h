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
#include "cmaf_stream.h"

//====================================================================================================
// CmafApplication
//====================================================================================================
class CmafApplication : public Application
{
public:
	static std::shared_ptr<CmafApplication> Create(const info::Application &application_info,
			const std::shared_ptr<ICmafChunkedTransfer> &chunked_transfer);

    CmafApplication(const info::Application &application_info,
    		const std::shared_ptr<ICmafChunkedTransfer> &chunked_transfer);

	virtual ~CmafApplication() final;

private:
	bool Start() override;
	bool Stop() override;

	// Application Implementation
	std::shared_ptr<Stream> CreateStream(std::shared_ptr<StreamInfo> info, uint32_t worker_count) override;
	bool DeleteStream(std::shared_ptr<StreamInfo> info) override;

private :
    int _segment_count;
    int _segment_duration;

	std::shared_ptr<ICmafChunkedTransfer> _chunked_transfer = nullptr;

};
