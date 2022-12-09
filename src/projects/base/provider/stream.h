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
		virtual bool Terminate();

		bool SendDataFrame(int64_t timestamp, const cmn::BitstreamFormat &format, const cmn::PacketType &packet_type, const std::shared_ptr<ov::Data> &frame);

	protected:
		Stream(const std::shared_ptr<pvd::Application> &application, StreamSourceType source_type);
		Stream(const std::shared_ptr<pvd::Application> &application, info::stream_id_t stream_id, StreamSourceType source_type);
		Stream(const std::shared_ptr<pvd::Application> &application, const info::Stream &stream_info);
		Stream(StreamSourceType source_type);

		virtual ~Stream();

		bool SetState(State state);
		bool SendFrame(const std::shared_ptr<MediaPacket> &packet);

		void ResetSourceStreamTimestamp();
		int64_t AdjustTimestampByBase(uint32_t track_id, int64_t pts,  int64_t dts, int64_t max_timestamp);
		int64_t AdjustTimestampByDelta(uint32_t track_id, int64_t timestamp, int64_t max_timestamp);
		int64_t GetDeltaTimestamp(uint32_t track_id, int64_t timestamp, int64_t max_timestamp);
		int64_t GetBaseTimestamp(uint32_t track_id);
		std::shared_ptr<pvd::Application> _application = nullptr;
		void UpdateReconnectTimeToBasetime();
	
	private:
		// TrackID : Timestamp(us)
		std::map<uint32_t, int64_t>			_source_timestamp_map;
		std::map<uint32_t, int64_t>			_last_timestamp_map;
		std::map<uint32_t, int64_t>			_base_timestamp_map;

		int64_t								_start_timestamp = -1LL;
		std::chrono::time_point<std::chrono::system_clock>	_last_pkt_received_time = std::chrono::time_point<std::chrono::system_clock>::min();

		State 	_state = State::IDLE;
	};
}