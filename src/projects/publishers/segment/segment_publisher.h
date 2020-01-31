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
#include <base/media_route/media_route_application_interface.h>
#include <base/publisher/publisher.h>
#include <config/config.h>
#include <publishers/segment/segment_stream/segment_stream_server.h>

#define DEFAULT_SEGMENT_WORKER_THREAD_COUNT		4

class SegmentPublisher : public pub::Publisher, public SegmentStreamObserver
{
protected:
	// This is used to prevent create new instance without factory class
	struct PrivateToken
	{
	};

public:
	template <typename Tpublisher>
	static std::shared_ptr<Tpublisher> Create(std::map<int, std::shared_ptr<HttpServer>> &http_server_manager,
											  const cfg::Server &server_config,
											  const info::Host &host_info,
											  const std::shared_ptr<MediaRouteInterface> &router)
	{
		auto publisher = std::make_shared<Tpublisher>((PrivateToken){}, server_config, host_info, router);

		auto instance = std::static_pointer_cast<SegmentPublisher>(publisher);
		if (instance->Start(http_server_manager) == false)
		{
			return nullptr;
		}

		return publisher;
	}

	bool GetMonitoringCollectionData(std::vector<std::shared_ptr<pub::MonitoringCollectionData>> &collections) override;

protected:
	SegmentPublisher(const cfg::Server &server_config, const info::Host &host_info, const std::shared_ptr<MediaRouteInterface> &router);
	~SegmentPublisher() override;

	virtual bool Start(std::map<int, std::shared_ptr<HttpServer>> &http_server_manager) = 0;

	//--------------------------------------------------------------------
	// Implementation of SegmentStreamObserver
	//--------------------------------------------------------------------
	bool OnPlayListRequest(const ov::String &app_name, const ov::String &stream_name,
						   const ov::String &file_name,
						   ov::String &play_list) override;

	bool OnSegmentRequest(const ov::String &app_name, const ov::String &stream_name,
						  const ov::String &file_name,
						  std::shared_ptr<SegmentData> &segment) override;

	std::shared_ptr<SegmentStreamServer> _stream_server = nullptr;
};
