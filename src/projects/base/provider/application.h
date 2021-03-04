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

#include "base/mediarouter/media_route_application_connector.h"
#include "stream.h"

#include <shared_mutex>

namespace pvd
{
	class Provider;
	
	class Application : public info::Application, public MediaRouteApplicationConnector
	{
	public:
		enum class ApplicationState : int8_t
		{
			Idle,
			Started,
			Stopped,
			Error
		};

		virtual bool Start();
		virtual bool Stop();

		const std::shared_ptr<Stream> GetStreamById(uint32_t stream_id);
		const std::shared_ptr<Stream> GetStreamByName(ov::String stream_name);

		uint32_t 	IssueUniqueStreamId();

		virtual bool AddStream(const std::shared_ptr<Stream> &stream);
		virtual bool DeleteStream(const std::shared_ptr<Stream> &stream);
		virtual bool DeleteAllStreams();

		const char* GetApplicationTypeName() final;

		std::shared_ptr<Provider> GetParentProvider()
		{
			return _provider;
		}

		ApplicationState GetState()
		{
			return _state;
		}

	protected:
		Application(const std::shared_ptr<Provider> &provider, const info::Application &application_info);
		virtual ~Application() override;
	
		virtual bool NotifyStreamCreated(const std::shared_ptr<Stream> &stream);
		virtual bool NotifyStreamDeleted(const std::shared_ptr<Stream> &stream);

		std::shared_mutex _streams_guard;
		std::map<uint32_t, std::shared_ptr<Stream>> _streams;

	private:
		std::shared_ptr<Provider> _provider;
		ApplicationState		_state = ApplicationState::Idle;
		std::atomic<info::stream_id_t>	_last_issued_stream_id { 0 };
	};
}
