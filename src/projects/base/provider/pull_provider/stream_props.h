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
			: _persist(false), _failback(false), _failback_timeout(-1), _no_input_failover_timeout(-1), _unused_stream_deletion_timeout(-1) {};

		PullStreamProperties(bool persist, bool failback, int32_t failback_timeout = -1, int32_t no_input_failover_timeout = -1, int32_t unused_stream_deletion_timeout = -1)
		{
			_persist = persist;
			_failback = failback;
			_failback_timeout = failback_timeout;
			_no_input_failover_timeout = no_input_failover_timeout;
			_unused_stream_deletion_timeout = unused_stream_deletion_timeout;
		};

		bool IsPersist()
		{
			return _persist;
		}

		bool IsFailback()
		{
			return _failback;
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

	private:
		bool _persist;

		bool _failback;

		int32_t _failback_timeout;

		int32_t _no_input_failover_timeout;

		int32_t _unused_stream_deletion_timeout;
	
	public:
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