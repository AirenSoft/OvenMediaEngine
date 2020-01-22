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
		virtual bool Start();
		virtual bool Stop();

		std::shared_ptr<Stream> GetStreamById(uint32_t stream_id);
		std::shared_ptr<Stream> GetStreamByName(ov::String stream_name);

		bool NotifyStreamCreated(std::shared_ptr<Stream> stream);
		bool NotifyStreamDeleted(std::shared_ptr<Stream> stream);

		uint32_t 	IssueUniqueStreamId();

		bool DeleteAllStreams();
		bool DeleteTerminatedStreams();

	protected:
		explicit Application(const info::Application &application_info);
		~Application() override;

		std::map<uint32_t, std::shared_ptr<Stream>> _streams;

	private:
		std::mutex 				_queue_guard;
		std::condition_variable	_queue_cv;
		uint32_t 				_last_issued_stream_id;
		std::mutex 				_streams_map_guard;
	};
}
