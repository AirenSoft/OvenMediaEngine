//==============================================================================
//
//  PullStream Properties Class
//
//  Created by Keukhan
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

namespace pvd
{
	class PullStreamProperties
	{
	public:
		PullStreamProperties()
			: _persistent(false), _failback(false), _relay(false), _failback_timeout(-1), _no_input_failover_timeout(-1), _unused_stream_deletion_timeout(-1), _retry_connect_count(2) {};

		PullStreamProperties(bool persistent, bool failback, bool relay, int32_t failback_timeout = -1, int32_t no_input_failover_timeout = -1, int32_t unused_stream_deletion_timeout = -1, int32_t retry_connect_count = 2)
		{
			_persistent = persistent;
			_failback = failback;
			_relay = relay;
			_failback_timeout = failback_timeout;
			_no_input_failover_timeout = no_input_failover_timeout;
			_unused_stream_deletion_timeout = unused_stream_deletion_timeout;
			_retry_connect_count = retry_connect_count;
		};

		bool IsPersistent()
		{
			return _persistent;
		}

		bool IsFailback()
		{
			return _failback;
		}

		bool IsRelay() {
			return _relay;
		}

		void SetFailback(bool failback)
		{
			_failback = failback;
		}

		void SetPersistent(bool persistent)
		{
			_persistent = persistent;
		}

		void SetRelay(bool relay)
		{
			_relay = relay;
		}

		int32_t GetFailbackTimeout()
		{
			return _failback_timeout;
		}

		int32_t GetNoInputFailoverTimeout()
		{
			return _no_input_failover_timeout;
		}

		int32_t GetUnusedStreamDeletionTimeout()
		{
			return _unused_stream_deletion_timeout;
		}

		int32_t GetRetryConnectCount() 
		{
			return _retry_connect_count;
		}

		void SetRetryConnectCount(int32_t retry_connect_count) 
		{
			_retry_connect_count = retry_connect_count;
		}

	private:
		bool _persistent;

		bool _failback;

		bool _relay;

		// not used yet. using the Origins->Properties option instead.
		int32_t _failback_timeout;

		// not used yet. using the Origins->Properties option instead.
		int32_t _no_input_failover_timeout;

		// not used yet. using the Origins->Properties option instead.
		int32_t _unused_stream_deletion_timeout;

		int32_t _retry_connect_count;

	public:
		// The last checked time is saved for failback.
		// TODO: However, it doesn't seem suitable for use in this class. This should be moved to another class.
		void UpdateFailbackCheckTime()
		{
			_last_failback_check_time = std::chrono::system_clock::now();
		}

		std::chrono::system_clock::time_point GetLastFailbackCheckTime()
		{
			return _last_failback_check_time;
		}

	private:
		std::chrono::system_clock::time_point _last_failback_check_time;
	};

}  // namespace pvd