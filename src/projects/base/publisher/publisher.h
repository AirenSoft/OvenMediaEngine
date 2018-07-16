#pragma once

#include <base/application/publisher_info.h>
#include <base/common_types.h>
#include "physical_port/physical_port.h"
#include "ice/ice_port_manager.h"
#include "base/publisher/application.h"
#include "base/publisher/stream.h"
#include "base/media_route/media_route_application_observer.h"

// WebRTC, HLS, MPEG-DASH 등 모든 Publisher는 다음 Interface를 구현하여 MediaRouterInterface에 자신을 등록한다.
class Publisher
{
public:
	virtual bool Start(std::vector<std::shared_ptr<ApplicationInfo>> &application_infos);
	virtual bool Stop();

	// app_name으로 Application을 찾아서 반환한다.
	std::shared_ptr<Application> GetApplication(ov::String app_name);
	std::shared_ptr<Stream> GetStream(ov::String app_name, ov::String stream_name);

	std::shared_ptr<Application> GetApplication(uint32_t app_id);
	std::shared_ptr<Stream> GetStream(uint32_t app_id, uint32_t stream_id);

protected:
	Publisher(std::shared_ptr<MediaRouteInterface> router);
	virtual ~Publisher();

	std::map<uint32_t, std::shared_ptr<Application>> _applications;

private:
	// 모든 Publisher는 Type을 정의해야 하며, Config과 일치해야 한다.
	virtual PublisherType GetPublisherType() = 0;
	virtual std::shared_ptr<Application> OnCreateApplication(const std::shared_ptr<ApplicationInfo> &info) = 0;

	std::shared_ptr<MediaRouteInterface> _router;
};

