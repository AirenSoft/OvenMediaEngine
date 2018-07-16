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
		std::shared_ptr<Application> GetApplication(ov::String app_name);
		std::shared_ptr<Stream> GetStream(ov::String app_name, ov::String stream_name);

		std::shared_ptr<Application> GetApplication(uint32_t app_id);
		std::shared_ptr<Stream> GetStream(uint32_t app_id, uint32_t stream_id);

	protected:
		Provider(std::shared_ptr<MediaRouteInterface> router);
		virtual ~Provider();

		std::map<uint32_t, std::shared_ptr<Application>> _applications;

	private:
		// 모든 Provider는 Name을 정의해야 하며, Config과 일치해야 한다.
		virtual ov::String GetProviderName() = 0;
		virtual std::shared_ptr<Application> OnCreateApplication(ApplicationInfo &info) = 0;

		std::shared_ptr<MediaRouteInterface> _router;
	};

}