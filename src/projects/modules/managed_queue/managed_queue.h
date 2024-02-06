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

#define SKIP_MESSAGE_ENABLED 1
#define SKIP_MESSAGE_CHECK_INTERVAL 500					 // 0.5 sec
#define SKIP_MESSAGE_STABLE_FOR_RETRIEVE_INTERVAL 10000	 // 10 sec
#define SKIP_MESSAGE_LOG_INTERVAL 5000					 // 1 sec

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
			  _stop(false),
			  _skip_message_enabled(false)
		{
			info::ManagedQueue::SetUrn(urn, Demangle(typeid(T).name()).CStr());

			_timer.Start();

			// Register to the server metrics
			// If the Unique id is duplicated or memory allocation failed, retry
			while (true)
			{
				SetId(IssueUniqueQueueId());

				if (MonitorInstance->GetServerMetrics()->OnQueueCreated(*this) == true)
				{
					break;
				}
			}
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
			EnqueueInternal(new ManagedQueueNode(item));
		}

		void Enqueue(T&& item)
		{
			EnqueueInternal(new ManagedQueueNode(std::move(item)));
		}

		void EnqueueInternal(ManagedQueueNode* node, int timeout = Infinite)
		{
			auto unique_lock = std::unique_lock(_mutex);			

			if (!node)
			{
				logc(LOG_TAG, "Failed to allocate memory. id:%u", GetId());
				return;
			}

			// Update statistics of input message count
			_input_message_count++;

#if SKIP_MESSAGE_ENABLED
			if (_skip_message_enabled == true)
			{
				auto curr_time = ov::Time::GetTimestampInMs();
				auto elapsed_check_time = curr_time - _skip_messages_last_check_time;
				auto elapsed_stable_time = curr_time - _skip_messages_last_changed_time;

				if (elapsed_check_time > SKIP_MESSAGE_CHECK_INTERVAL)
				{
					_skip_messages_last_check_time = curr_time;

					if ((_size > _threshold / 10) &&				   // It exceeded 10% of the threshold
						(_size >= _skip_message_previous_queue_size))  // The queue size has increased compared to before
					{
						if (_skip_message_count < 10)  // Maximum skip count is 10
						{
							_skip_message_count++;
						}

						_skip_message_previous_queue_size = _size;
						_skip_messages_last_changed_time = curr_time;

						auto shared_lock = std::shared_lock(_name_mutex);
						logw(LOG_TAG, "[%s] Managed queue is unstable. q.size(%d), q.threshold(%d) q.imps(%d), q.omps(%d), skip(%d)", _urn->ToString().CStr(),
							 GetSize(), GetThreshold(), _input_message_per_second, _output_message_per_second, _skip_message_count);
					}
					// If the queue is stable, slowly decrease the number of skip frames.
					else if ((_skip_message_count > 0) &&										   // Skip Message is operating
							 (elapsed_stable_time > SKIP_MESSAGE_STABLE_FOR_RETRIEVE_INTERVAL) &&  // It appears to be in a stable state.
							 (_size <= 1))														   // The queue size is 1 or less.
					{
						if (--_skip_message_count < 0)
						{
							_skip_message_count = 0;
						}

						_skip_message_previous_queue_size = _size;
						_skip_messages_last_changed_time = curr_time;

						auto shared_lock = std::shared_lock(_name_mutex);
						logi(LOG_TAG, "[%s] Managed queue is stable. q.size(%d), q.threshold(%d) q.imps(%d), q.omps(%d), skip(%d)", _urn->ToString().CStr(),
							 GetSize(), GetThreshold(), _input_message_per_second, _output_message_per_second, _skip_message_count);
					}
				}

				if ((_skip_message_count > 0) && (_input_message_count % (_skip_message_count + 1) != 0))
				{
					if (curr_time - _skip_messages_last_log_time > SKIP_MESSAGE_LOG_INTERVAL)
					{
						_skip_messages_last_log_time = curr_time;

						auto shared_lock = std::shared_lock(_name_mutex);
						logw(LOG_TAG, "[%s] Drop a message by message skip. q.size(%d), q.threshold(%d) q.imps(%d), q.omps(%d), skip(%d)", _urn->ToString().CStr(),
							 GetSize(), GetThreshold(), _input_message_per_second, _output_message_per_second, _skip_message_count);
					}

					_drop_message_count++;

					delete node;

					_condition.notify_all();

					return;
				}

				// If the queue exceeds the threshold, drop the frame.
				if (IsThresholdExceeded() == true)
				{
					if (curr_time - _skip_messages_last_log_time > SKIP_MESSAGE_LOG_INTERVAL)
					{
						_skip_messages_last_log_time = curr_time;

						auto shared_lock = std::shared_lock(_name_mutex);
						logw(LOG_TAG, "[%s] Managed queue is exceed. drop message. q.size(%d), q.threshold(%d) q.imps(%d), q.omps(%d), skip(%d)", _urn->ToString().CStr(),
							 GetSize(), GetThreshold(), _input_message_per_second, _output_message_per_second, _skip_message_count);
					}

					_drop_message_count++;
					
					delete node;

					_condition.notify_all();

					return;
				}

				// logi(LOG_TAG, "q.size(%d), q.imps(%d), q.omps(%d), skip(%d), stable(%d)", _size,  _skip_message_count, elapsed_stable_time);
			}
#endif

			if(_prevent_exceed_threshold_and_wait_enabled == true)
			{
				// Wait until the queue size is less than threshold
				std::chrono::system_clock::time_point expire = (timeout == Infinite) ? std::chrono::system_clock::time_point::max() : std::chrono::system_clock::now() + std::chrono::milliseconds(timeout);
				auto result = _condition.wait_until(unique_lock, expire, [this]() -> bool {
					return (_size  < _threshold);
				});
				if (!result || _stop)
				{
					loge(LOG_TAG, "[%s] queue is full. q.size(%d), q.threshold(%d)", _urn->ToString().CStr(), _size, _threshold);
					delete node;
					return;
				}
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

			if(_prevent_exceed_threshold_and_wait_enabled == true)
			{
				_condition.notify_all();
			}

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

			ClearMetrics();
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

			ClearMetrics();

			_condition.notify_all();
		}

		bool IsStopped() const
		{
			return _stop;
		}

		// Skip message for performance failure recovery
		// If the queue size increases due to insufficient performance, the problem is avoided by dropping the message.
		// * Warning: This should only be used where dropping a message will not affect the operation. example) video encoder, video scaler
		void SetSkipMessageEnable(bool enable)
		{
			_skip_message_enabled = enable;
		}

		void SetPreventExceedThreshold(bool enable)
		{
			_prevent_exceed_threshold_and_wait_enabled = enable;
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

			if (_timer.IsElapsed(_stats_metric_interval) && _timer.Update())
			{
				// Update statistics of message per second
				_input_message_per_second = (_input_message_count - _last_input_message_count) * (1000 / _stats_metric_interval);
				_output_message_per_second = (_output_message_count - _last_output_message_count) * (1000 / _stats_metric_interval);
				_last_input_message_count = _input_message_count;
				_last_output_message_count = _output_message_count;

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

		void ClearMetrics()
		{
			_peak = 0;
			_input_message_per_second = 0;
			_output_message_per_second = 0;
			_input_message_count = 0;
			_output_message_count = 0;
			_threshold_exceeded_time_in_us = 0;

			MonitorInstance->GetServerMetrics()->OnQueueUpdated(*this);
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

		// Message Skip for lack of performance
		bool _skip_message_enabled = false;
		int64_t _skip_messages_last_check_time = ov::Time::GetTimestampInMs();
		int64_t _skip_messages_last_changed_time = ov::Time::GetTimestampInMs();
		int64_t _skip_messages_last_log_time = 0;
		int32_t _skip_message_count = 0;
		size_t _skip_message_previous_queue_size = 0;

		// Prevent exceed threshold. If true, the queue will not exceed the threshold
		// Wait until the queue falls below the threshold
		bool _prevent_exceed_threshold_and_wait_enabled = false;
	};

}  // namespace ov