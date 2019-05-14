//==============================================================================
//
//  Provider Base Class 
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "config/config.h"

#include "base/common_types.h"
#include "base/ovlibrary/ovlibrary.h"

#include "base/media_route/media_route_application_connector.h"

namespace pvd
{
	class Stream;

	class Application : public info::Application, public MediaRouteApplicationConnector
	{
		enum class ApplicationState : int8_t
		{
			Idle,
			Started,
			Stopped,
			Error
		};

	public:
		bool Start();
		bool Stop();

		std::shared_ptr<Stream> GetStreamById(uint32_t stream_id);
		std::shared_ptr<Stream> GetStreamByName(ov::String stream_name);

		std::shared_ptr<Stream> MakeStream();
		bool CreateStream2(std::shared_ptr<Stream> stream);
		bool DeleteStream2(std::shared_ptr<Stream> stream);

		// 상위 클래스에서 Stream 객체를 생성해서 받아옴
		virtual std::shared_ptr<Stream> OnCreateStream() = 0;

	protected:
		explicit Application(const info::Application *application_info);
		~Application() override;

		std::map<uint32_t, std::shared_ptr<Stream>> _streams;

	private:
		std::mutex _queue_guard;
		std::condition_variable _queue_cv;
	};
}
