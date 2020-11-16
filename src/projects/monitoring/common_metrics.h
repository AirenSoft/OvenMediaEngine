//
// Created by getroot on 20. 1. 31.
//

#pragma once

#include "base/common_types.h"
#include "base/info/info.h"
#include "base/info/stream.h"

namespace mon
{
	class CommonMetrics
	{
	public:
		virtual ov::String GetInfoString();
		virtual void ShowInfo();

		uint32_t GetUnusedTimeSec() const;
		const std::chrono::system_clock::time_point& GetCreatedTime() const;
		const std::chrono::system_clock::time_point& GetLastUpdatedTime() const;
		
		virtual uint64_t GetTotalBytesIn() const;
		virtual uint64_t GetTotalBytesOut() const;
		virtual uint32_t GetTotalConnections() const;
		virtual uint32_t GetMaxTotalConnections() const;
		virtual std::chrono::system_clock::time_point GetMaxTotalConnectionsTime() const;
		virtual std::chrono::system_clock::time_point GetLastRecvTime() const;
		virtual std::chrono::system_clock::time_point GetLastSentTime() const;
		virtual uint64_t GetBytesOut(PublisherType type) const;
		virtual uint64_t GetConnections(PublisherType type) const;
		
		virtual void IncreaseBytesIn(uint64_t value);
		virtual void IncreaseBytesOut(PublisherType type, uint64_t value);
		virtual void OnSessionConnected(PublisherType type);
		virtual void OnSessionDisconnected(PublisherType type);

	protected:
		CommonMetrics();
		~CommonMetrics() = default;

		// Renew last updated time
		void UpdateDate();

		std::chrono::system_clock::time_point _created_time;
		std::chrono::system_clock::time_point _last_updated_time;

		// From Provider
		std::atomic<uint64_t> _total_bytes_in;

		// From Publishers
		std::atomic<uint64_t> _total_bytes_out;
		std::atomic<uint32_t> _total_connections;
		
		std::atomic<uint32_t> _max_total_connections;
		// Time to reach maximum number of connections. 
		// TODO(Getroot): Does it need mutex? Check!
		std::chrono::system_clock::time_point	_max_total_connection_time;
		std::chrono::system_clock::time_point	_last_recv_time;
		std::chrono::system_clock::time_point	_last_sent_time;

		// From Publishers
		class PublisherMetrics
		{
		public:
			std::atomic<uint64_t> _bytes_out;
			std::atomic<uint32_t> _connections;
		};

		PublisherMetrics _publisher_metrics[static_cast<int8_t>(PublisherType::NumberOfPublishers)];
	};
}  // namespace mon