#pragma once

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
	virtual bool Start();
	virtual bool Stop();

	// app_name으로 Application을 찾아서 반환한다.
	std::shared_ptr<Application> GetApplicationByName(ov::String app_name);
	std::shared_ptr<Stream> GetStream(ov::String app_name, ov::String stream_name);

	std::shared_ptr<Application> GetApplicationById(info::application_id_t application_id);
	std::shared_ptr<Stream> GetStream(info::application_id_t application_id, uint32_t stream_id);

protected:
	explicit Publisher(const info::Application &application_info, std::shared_ptr<MediaRouteInterface> router);
	virtual ~Publisher() = default;

	template<typename Tpublisher>
	const Tpublisher *FindPublisherInfo()
	{
		const auto &publishers = _application_info.GetPublishers();

		for(auto &publisher_info : publishers)
		{
			if(GetPublisherType() == publisher_info->GetType())
			{
				return dynamic_cast<const Tpublisher *>(publisher_info);
			}
		}

		return nullptr;
	}

	// 모든 Publisher는 Type을 정의해야 하며, Config과 일치해야 한다.
	virtual cfg::PublisherType GetPublisherType() = 0;
	virtual std::shared_ptr<Application> OnCreateApplication(const info::Application &application_info) = 0;

	// 모든 application들의 map
	std::map<info::application_id_t, std::shared_ptr<Application>> _applications;

	// Publisher를 상속받은 클래스에서 사용되는 정보
	std::shared_ptr<MediaRouteApplicationInterface> _application;
	info::Application _application_info;

	std::shared_ptr<MediaRouteInterface> _router;
};

