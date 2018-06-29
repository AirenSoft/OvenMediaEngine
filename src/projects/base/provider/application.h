//==============================================================================
//
//  Provider Base Class 
//
//  Created by Kwon Keuk Hanb
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "config/config.h"

#include "base/common_types.h"
#include "base/ovlibrary/ovlibrary.h"
#include "base/application/application.h"

#include "base/media_route/media_route_application_connector.h"

namespace pvd
{
	class Application;
	class Stream;

	class Application : public ApplicationInfo, public MediaRouteApplicationConnector
	{
		enum ApplicationState
		{
			kIdel,
			kStarted,
			kStopped,
			kError
		};

	public:
		bool 	Start();
		bool 	Stop();

		std::shared_ptr<Stream> GetStream(uint32_t stream_id);
		std::shared_ptr<Stream> GetStream(ov::String stream_name);

		std::shared_ptr<Stream> MakeStream();
		bool	CreateStream2(std::shared_ptr<Stream> stream);
		bool	DeleteStream2(std::shared_ptr<Stream> stream);

		// 상위 클래스에서 Stream 객체를 생성해서 받아옴
		virtual std::shared_ptr<Stream> OnCreateStream() = 0;
		
	protected:
		explicit Application(const ApplicationInfo &info);
		virtual ~Application();

		std::map<uint32_t, std::shared_ptr<Stream>> _streams;

	private:
		std::mutex _queue_guard;
		std::condition_variable _queue_cv;
	};
}
