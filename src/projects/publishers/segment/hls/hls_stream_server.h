//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "../segment_stream/segment_stream_server.h"
#include "hls_interceptor.h"

class HlsStreamServer : public SegmentStreamServer
{
public:
	PublisherType GetPublisherType() const noexcept override
	{
		return PublisherType::Hls;
	}

	const char *GetPublisherName() const noexcept override
	{
		return "HLS Publisher";
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
	HttpConnection ProcessStreamRequest(const std::shared_ptr<HttpClient> &client,
										const SegmentStreamRequestInfo &request_info,
										const ov::String &file_ext) override;

	HttpConnection ProcessPlayListRequest(const std::shared_ptr<HttpClient> &client,
										  const SegmentStreamRequestInfo &request_info,
										  PlayListType play_list_type) override;

	HttpConnection ProcessSegmentRequest(const std::shared_ptr<HttpClient> &client,
										 const SegmentStreamRequestInfo &request_info,
										 SegmentType segment_type) override;
};
