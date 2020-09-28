//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <publishers/segment/segment_stream/segment_stream_server.h>

#include "dash_interceptor.h"
#include "dash_packetizer.h"

class DashStreamServer : public SegmentStreamServer
{
public:
	PublisherType GetPublisherType() const noexcept override
	{
		return PublisherType::Dash;
	}

	const char *GetPublisherName() const noexcept override
	{
		return "DASH Publisher";
	}

	std::shared_ptr<SegmentStreamInterceptor> CreateInterceptor() override
	{
		return std::make_shared<DashInterceptor>();
	}

protected:
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
