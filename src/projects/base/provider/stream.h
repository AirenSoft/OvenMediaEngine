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
#include "base/info/stream.h"
#include "monitoring/monitoring.h"

#include <base/mediarouter/media_buffer.h>

namespace pvd
{
	class Application;

	class Stream : public info::Stream, public ov::EnableSharedFromThis<Stream>
	{
	public:
		enum class State
		{
			IDLE,
			CONNECTED,
			DESCRIBED,
			PLAYING,
			STOPPED,	// will be retried, Set super class
			ERROR,		// will be retried
			TERMINATED	// will be deleted, Set super class
		};

		State GetState(){return _state;};

		void SetApplication(const std::shared_ptr<pvd::Application> &application)
		{
			_application = application;
		}

		const char* GetApplicationTypeName();

		const std::shared_ptr<pvd::Application> &GetApplication()
		{
			return _application;
		}

		std::shared_ptr<const pvd::Application> GetApplication() const
		{
			return _application;
		}

		virtual bool Start();
		virtual bool Stop();

	protected:
		Stream(const std::shared_ptr<pvd::Application> &application, StreamSourceType source_type);
		Stream(const std::shared_ptr<pvd::Application> &application, info::stream_id_t stream_id, StreamSourceType source_type);
		Stream(const std::shared_ptr<pvd::Application> &application, const info::Stream &stream_info);
		Stream(StreamSourceType source_type);

		virtual ~Stream();

		bool SetState(State state);
		bool SendFrame(const std::shared_ptr<MediaPacket> &packet);

		void ResetSourceStreamTimestamp();
		uint64_t AdjustTimestampByBase(uint32_t track_id, uint64_t timestamp, uint64_t max_timestamp);
		uint64_t AdjustTimestampByDelta(uint32_t track_id, uint64_t timestamp, uint64_t max_timestamp);
		uint64_t GetDeltaTimestamp(uint32_t track_id, uint64_t timestamp, uint64_t max_timestamp);
		uint64_t GetBaseTimestamp(uint32_t track_id);
		std::shared_ptr<pvd::Application> _application = nullptr;

	private:
		// Track ID : Timestamp
		std::map<uint32_t, uint64_t>			_source_timestamp_map;
		std::map<uint32_t, uint64_t>			_last_timestamp_map;
		std::map<uint32_t, uint64_t>			_base_timestamp_map;

		State 	_state = State::IDLE;
	};
}