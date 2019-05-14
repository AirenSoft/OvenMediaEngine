//==============================================================================
//
//  Provider Base Class 
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "base/common_types.h"
#include "base/media_route/media_route_interface.h"

namespace pvd
{
	class Application;
	class Stream;

	// WebRTC, HLS, MPEG-DASH 등 모든 Provider는 다음 Interface를 구현하여 MediaRouterInterface에 자신을 등록한다.
	class Provider
	{
	public:
		virtual bool Start();
		virtual bool Stop();

		// app_name으로 Application을 찾아서 반환한다.
		std::shared_ptr<Application> GetApplicationByName(ov::String app_name);
		std::shared_ptr<Stream> GetStreamByName(ov::String app_name, ov::String stream_name);

		std::shared_ptr<Application> GetApplicationById(info::application_id_t app_id);
		std::shared_ptr<Stream> GetStreamById(info::application_id_t app_id, uint32_t stream_id);

	protected:
		Provider(const info::Application *application_info, std::shared_ptr<MediaRouteInterface> router);
		virtual ~Provider();

		// 모든 Provider는 Name을 정의해야 하며, Config과 일치해야 한다.
		virtual cfg::ProviderType GetProviderType() = 0;
		virtual std::shared_ptr<Application> OnCreateApplication(const info::Application *application_info) = 0;

		const info::Application *_application_info;
		std::map<info::application_id_t, std::shared_ptr<Application>> _applications;

		std::shared_ptr<MediaRouteInterface> _router;
	};

}