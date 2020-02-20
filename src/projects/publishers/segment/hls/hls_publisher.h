//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "../segment_publisher.h"

class HlsPublisher : public SegmentPublisher
{
public:
	static std::shared_ptr<HlsPublisher> Create(std::map<int, std::shared_ptr<HttpServer>> &http_server_manager,
												const cfg::Server &server_config,
												const info::Host &host_info,
												const std::shared_ptr<MediaRouteInterface> &router);

	HlsPublisher(PrivateToken token, const cfg::Server &server_config, const info::Host &host_info, const std::shared_ptr<MediaRouteInterface> &router);

protected:
	bool HandleSignedUrl(const std::shared_ptr<HttpClient> &client, const std::shared_ptr<const ov::Url> &request_url);

	//--------------------------------------------------------------------
	// Implementation of SegmentPublisher
	//--------------------------------------------------------------------
	bool Start(std::map<int, std::shared_ptr<HttpServer>> &http_server_manager) override;

	//--------------------------------------------------------------------
	// Implementation of SegmentStreamObserver
	//--------------------------------------------------------------------
	bool OnPlayListRequest(const std::shared_ptr<HttpClient> &client,
						   const ov::String &app_name, const ov::String &stream_name,
						   const ov::String &file_name,
						   ov::String &play_list) override;

	//--------------------------------------------------------------------
	// Implementation of Publisher
	//--------------------------------------------------------------------
	std::shared_ptr<pub::Application> OnCreatePublisherApplication(const info::Application &application_info) override;
	PublisherType GetPublisherType() const override

	{
		return PublisherType::Hls;
	}

	const char *GetPublisherName() const override
	{
		return "HLS";
	}
};
