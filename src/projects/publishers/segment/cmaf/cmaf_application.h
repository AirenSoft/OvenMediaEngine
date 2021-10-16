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

#include "../segment_stream/segment_stream.h"

class CmafApplication : public pub::Application
{
public:
	static std::shared_ptr<CmafApplication> Create(const std::shared_ptr<pub::Publisher> &publisher, const info::Application &application_info,
												   const std::shared_ptr<ChunkedTransferInterface> &chunked_transfer);

	CmafApplication(const std::shared_ptr<pub::Publisher> &publisher,
					const info::Application &application_info,
					const std::shared_ptr<ChunkedTransferInterface> &chunked_transfer);

	virtual ~CmafApplication() final;

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

private:
	int _segment_count;
	int _segment_duration;

	ov::String _utc_timing_scheme;
	ov::String _utc_timing_value;

	std::shared_ptr<ChunkedTransferInterface> _chunked_transfer = nullptr;
};
