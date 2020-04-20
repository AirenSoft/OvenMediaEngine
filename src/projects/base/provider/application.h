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
#include "stream.h"

#include <shared_mutex>

namespace pvd
{
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

		const std::map<uint32_t, std::shared_ptr<Stream>>& GetStreams() const;
		const std::shared_ptr<Stream> GetStreamById(uint32_t stream_id);
		const std::shared_ptr<Stream> GetStreamByName(ov::String stream_name);

		uint32_t 	IssueUniqueStreamId();

		// For receiving the push 
		std::shared_ptr<pvd::Stream> CreateStream(const uint32_t stream_id, const ov::String &stream_name, const std::vector<std::shared_ptr<MediaTrack>> &tracks);
		std::shared_ptr<pvd::Stream> CreateStream(const ov::String &stream_name, const std::vector<std::shared_ptr<MediaTrack>> &tracks);
		// For pulling
		std::shared_ptr<pvd::Stream> CreateStream(const ov::String &stream_name, const std::vector<ov::String> &url_list);

		// Delete stream
		virtual bool DeleteStream(std::shared_ptr<Stream> stream);

		bool DeleteAllStreams();
		bool DeleteTerminatedStreams();

		const char* GetApplicationTypeName() const override
		{
			return "Provider Base Application";
		}

	protected:
		explicit Application(const info::Application &application_info);
		~Application() override;

		// For child
		virtual std::shared_ptr<pvd::Stream> CreatePushStream(const uint32_t stream_id, const ov::String &stream_name) = 0;
		virtual std::shared_ptr<pvd::Stream> CreatePullStream(const uint32_t stream_id, const ov::String &stream_name, const std::vector<ov::String> &url_list) = 0;

		std::map<uint32_t, std::shared_ptr<Stream>> _streams;

	private:
		bool NotifyStreamCreated(std::shared_ptr<Stream> stream);
		bool NotifyStreamDeleted(std::shared_ptr<Stream> stream);

		std::mutex 				_queue_guard;
		std::condition_variable	_queue_cv;
		std::atomic<info::stream_id_t>	_last_issued_stream_id { 0 };
		std::shared_mutex 		_streams_map_guard;
		ApplicationState		_state = ApplicationState::Idle;
	};
}
