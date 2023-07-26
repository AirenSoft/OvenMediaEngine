//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Keukhan Kwon
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include <monitoring/monitoring.h>

#include <condition_variable>
#include <optional>
#include <queue>
#include <shared_mutex>

#include "base/info/managed_queue.h"
#include "base/ovlibrary/ovlibrary.h"

#define MANAGED_QUEUE_METRICS_UPDATE_INTERVAL_IN_MSEC 1000
#define MANAGED_QUEUE_LOG_INTERVAL_IN_MSEC 5000

namespace ov
{
	template <typename T>
	class ManagedQueue : public info::ManagedQueue
	{
	private:
		const char* LOG_TAG = "ManagedQueue";

		struct ManagedQueueNode
		{
			T data;

			ManagedQueueNode* next;

			std::chrono::high_resolution_clock::time_point _start;

			ManagedQueueNode(const T& value, ManagedQueueNode* next_node = nullptr)
				: data(value), next(next_node), _start(std::chrono::high_resolution_clock::time_point::min())
			{
				_start = std::chrono::high_resolution_clock::now();
			}
		};

	public:
		ManagedQueue()
			: ManagedQueue(nullptr) {}

		ManagedQueue(std::shared_ptr<info::ManagedQueue::URN> urn, size_t threshold = 0, int log_interval_in_msec = MANAGED_QUEUE_LOG_INTERVAL_IN_MSEC)
			: info::ManagedQueue(threshold),
			  _stats_metric_interval(MANAGED_QUEUE_METRICS_UPDATE_INTERVAL_IN_MSEC),
			  _log_interval(log_interval_in_msec),
			  _front_node(nullptr),
			  _rear_node(nullptr),
			  _stop(false)
		{
			SetId(ov::Random::GenerateUInt32() - 1);
			info::ManagedQueue::SetUrn(urn, Demangle(typeid(T).name()).CStr());

			_timer.Start();

			// Register to the server metrics
			MonitorInstance->GetServerMetrics()->OnQueueCreated(*this);
		}

		~ManagedQueue()
		{
			Clear();

			// Unregister to the server metrics
			MonitorInstance->GetServerMetrics()->OnQueueDeleted(*this);
		}

		void SetUrn(std::shared_ptr<info::ManagedQueue::URN> urn)
		{
			info::ManagedQueue::SetUrn(urn, Demangle(typeid(T).name()).CStr());

			MonitorInstance->GetServerMetrics()->OnQueueUpdated(*this, true);
		}

		void Enqueue(const T& item)
		{
			auto lock_guard = std::lock_guard(_mutex);

			EnqueueInternal(new ManagedQueueNode(item));
		}

		void Enqueue(T&& item)
		{
			auto lock_guard = std::lock_guard(_mutex);

			EnqueueInternal(new ManagedQueueNode(std::move(item)));
		}

		void EnqueueInternal(ManagedQueueNode* node)
		{
			if (!node)
			{
				logc(LOG_TAG, "Failed to allocate memory. id:%u", GetId());
				return;
			}

			if (_size == 0)
			{
				_front_node = node;
			}
			else
			{
				_rear_node->next = node;
			}

			_rear_node = node;

			_size++;

			// Update statistics of input message count
			_input_message_count++;

			UpdateMetrics();

			_condition.notify_all();
		}

		std::optional<T> Front(int timeout = Infinite)
		{
			auto unique_lock = std::unique_lock(_mutex);

			if (_stop)
			{
				return {};	// Stop is requested
			}

			if (_size == 0)
			{
				std::chrono::system_clock::time_point expire = (timeout == Infinite) ? std::chrono::system_clock::time_point::max() : std::chrono::system_clock::now() + std::chrono::milliseconds(timeout);

				auto result = _condition.wait_until(unique_lock, expire, [this]() -> bool {
					return (((_size == 0) == false) || _stop);
				});

				if (!result || _stop)
				{
					return {};	// timed out / Stop is requested
				}
			}

			return _front_node->data;
		}

		std::optional<T> Back(int timeout = Infinite)
		{
			auto unique_lock = std::unique_lock(_mutex);

			if (_stop)
			{
				return {};	// Stop is requested
			}

			if (_size == 0)
			{
				std::chrono::system_clock::time_point expire = (timeout == Infinite) ? std::chrono::system_clock::time_point::max() : std::chrono::system_clock::now() + std::chrono::milliseconds(timeout);

				auto result = _condition.wait_until(unique_lock, expire, [this]() -> bool {
					return (((_size == 0) == false) || _stop);
				});

				if (!result || _stop)
				{
					return {};	// timed out / Stop is requested
				}
			}

			return _rear_node->data;
		}

		std::optional<T> Dequeue(int timeout = Infinite)
		{
			auto unique_lock = std::unique_lock(_mutex);

			if (_stop)
			{
				return {};	// Stop is requested
			}

			if (_size == 0)
			{
				std::chrono::system_clock::time_point expire = (timeout == Infinite) ? std::chrono::system_clock::time_point::max() : std::chrono::system_clock::now() + std::chrono::milliseconds(timeout);
			
				auto result = _condition.wait_until(unique_lock, expire, [this]() -> bool {
					return (((_size == 0) == false) || _stop);
				});

				if (!result || _stop)
				{
					return {};	// timed out / Stop is requested
				}
			}

			ManagedQueueNode* node = _front_node;
			_front_node = _front_node->next;
			if (_front_node == nullptr)
			{
				_rear_node = nullptr;
			}

			T value = std::move(node->data);

			_size--;

			// Update statistics of output message count
			_output_message_count++;

			// Update statistics of waiting time (microseconds)
			if (node->_start != std::chrono::system_clock::time_point::max())
			{
				auto current = std::chrono::high_resolution_clock::now();
				_waiting_time_in_us = _waiting_time_in_us * 0.9 + std::chrono::duration_cast<std::chrono::microseconds>(current - node->_start).count() * 0.1;
			}

			delete node;

			UpdateMetrics();

			return value;
		}

		bool IsEmpty() const
		{
			auto lock_guard = std::lock_guard(_mutex);

			return (_size == 0);
		}

		// Cleared all items in the queue
		void Clear()
		{
			auto lock_guard = std::lock_guard(_mutex);

			while (_front_node != nullptr)
			{
				ManagedQueueNode* temp = _front_node;

				_front_node = _front_node->next;

				delete temp;
			}

			_rear_node = nullptr;

			_size = 0;
		}

		size_t Size() const
		{
			auto lock_guard = std::lock_guard(_mutex);

			return _size;
		}

		void Stop()
		{
			auto lock_guard = std::lock_guard(_mutex);

			_stop = true;

			_condition.notify_all();
		}

		bool IsStopped() const
		{
			return _stop;
		}

	protected:
		// Update statistical metrics and send data to monitoring module.
		void UpdateMetrics()
		{
			// Update the peak statistics
			if (_peak < _size)
			{
				_peak = _size;
			}

			if ((_timer.IsElapsed(_stats_metric_interval) && _timer.Update()) || (_size == 0))
			{
				// Update statistics of message per second
				_input_message_per_second = _input_message_count;
				_output_message_per_second = _output_message_count;
				_output_message_count = _input_message_count = 0;

				if ((_threshold > 0) && (_size >= _threshold))
				{
					_threshold_exceeded_time_in_us += _stats_metric_interval;

					// Logging
					_last_logging_time += _stats_metric_interval;
					if (_last_logging_time >= _log_interval)
					{
						_last_logging_time = 0;
						auto shared_lock = std::shared_lock(_name_mutex);
						logw(LOG_TAG, "[%u] %s size has exceeded the threshold: queue: %zu, threshold: %zu, peak: %zu", GetId(), _urn->ToString().CStr(), _size, _threshold, _peak);
					}
				}
				else
				{
					_threshold_exceeded_time_in_us = 0;
				}

				MonitorInstance->GetServerMetrics()->OnQueueUpdated(*this);
			}
		}

	private:
		StopWatch _timer;

		int _stats_metric_interval = 0;

		int _log_interval = 0;
		int64_t _last_logging_time = 0;

		// Linked list of the queue
		ManagedQueueNode* _front_node;
		ManagedQueueNode* _rear_node;

		// Mutex and condition variable for the queue
		mutable std::mutex _mutex;
		std::condition_variable _condition;

		// Stop flag
		bool _stop;
	};

}  // namespace ov