//
// Created by getroot on 20. 1. 16.
//

#pragma once

#include "base/info/info.h"
#include "server_metrics.h"
#include "event_logger.h"

#define MonitorInstance				mon::Monitoring::GetInstance()
#define HostMetrics(info)			mon::Monitoring::GetInstance()->GetHostMetrics(info);
#define ApplicationMetrics(info)	mon::Monitoring::GetInstance()->GetApplicationMetrics(info);
#define StreamMetrics(info)			mon::Monitoring::GetInstance()->GetStreamMetrics(info);

namespace mon
{
	class Monitoring : public ServerMetrics
	{
	public:
        static Monitoring *GetInstance()
	    {
            static Monitoring monitor;
            return &monitor;
	    }

		void SetLogPath(const ov::String &log_path);

		// Events
		void OnServerStarted(ov::String user_key, ov::String server_name, ov::String server_id);
		bool OnHostCreated(const info::Host &host_info);
		bool OnHostDeleted(const info::Host &host_info);
		bool OnApplicationCreated(const info::Application &app_info);
		bool OnApplicationDeleted(const info::Application &app_info);
		bool OnStreamCreated(const info::Stream &stream_info);
		bool OnStreamDeleted(const info::Stream &stream_info);
		bool OnStreamUpdated(const info::Stream &stream_info);

		void IncreaseBytesIn(const info::Stream &stream_info, uint64_t value);
		void IncreaseBytesOut(const info::Stream &stream_info, PublisherType type, uint64_t value);
		void OnSessionConnected(const info::Stream &stream_info, PublisherType type);
		void OnSessionDisconnected(const info::Stream &stream_info, PublisherType type);
		void OnSessionsDisconnected(const info::Stream &stream_info, PublisherType type, uint64_t number_of_sessions);

	private:
		ov::String _user_key;
		EventLogger	_logger;

	};
}  // namespace mon