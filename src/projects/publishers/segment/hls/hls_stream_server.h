//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "hls_interceptor.h"

#include <segment_stream/segment_stream_server.h>

class HlsStreamServer : public SegmentStreamServer
{
public:
	PublisherType GetPublisherType() const noexcept override
	{
		return PublisherType::Hls;
	}

	const char *GetPublisherName() const noexcept override
	{
		return "HLS";
	}

	std::shared_ptr<SegmentStreamInterceptor> CreateInterceptor() override
	{
		auto interceptor = std::make_shared<HlsInterceptor>();
		return std::static_pointer_cast<SegmentStreamInterceptor>(interceptor);
	}

protected:
	//--------------------------------------------------------------------
	// Implementation of SegmentStreamServer
	//--------------------------------------------------------------------
	HttpConnection ProcessRequestStream(const std::shared_ptr<HttpClient> &client,
										const ov::String &app_name, const ov::String &stream_name,
										const ov::String &file_name, const ov::String &file_ext) override;

	HttpConnection OnPlayListRequest(const std::shared_ptr<HttpClient> &client,
									 const ov::String &app_name, const ov::String &stream_name,
									 const ov::String &file_name,
									 PlayListType play_list_type) override;

	HttpConnection OnSegmentRequest(const std::shared_ptr<HttpClient> &client,
									const ov::String &app_name, const ov::String &stream_name,
									const ov::String &file_name,
									SegmentType segment_type) override;
};
