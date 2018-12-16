//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "event.h"

#include <cstdio>
#include <functional>
#include <thread>
#include <queue>
#include <mutex>
#include <algorithm>
#include <utility>

namespace ov
{
	// Return false if need to stop when using repeat mode
	typedef std::function<bool(void *parameter)> DelayQueueFunction;

	class DelayQueue
	{
	public:
		DelayQueue();
		virtual ~DelayQueue();

		// after의 단위는 ms
		void Push(const DelayQueueFunction &func, void *parameter, int after, bool repeat);
		void Push(const DelayQueueFunction &func, int after);
		ssize_t GetCount() const;

		bool Start();
		bool Stop();

	protected:
		void DispatchThreadProc();

		struct DelayQueueItem
		{
		public:
			int64_t index;

			DelayQueueFunction function;
			void *parameter;

			int after;
			bool repeat;

			std::chrono::time_point<std::chrono::system_clock> time_point;

			// after의 단위는 ms
			DelayQueueItem(int64_t index, DelayQueueFunction function, void *parameter, int after, bool repeat)
				: index(index),

				  function(std::move(function)),
				  parameter(parameter),

				  after(after),
				  repeat(repeat)
			{
				RecalculateTimePoint();
			}

			void RecalculateTimePoint()
			{
				time_point = std::chrono::system_clock::now() + std::chrono::milliseconds(after);
			}

			bool operator <(const DelayQueueItem &item) const
			{
				// interval이 비교 1순위, index가 비교 2순위
				return (time_point == item.time_point) ? (index > item.index) : (time_point > item.time_point);
			}
		};

		int64_t _index;

		std::thread _thread;
		volatile bool _stop;

		// 실행해야 할 항목들이 저장된 queue
		std::priority_queue<DelayQueueItem> _queue;
		// GetCount()로 개수를 얻어올 때 lock을 걸어야 하므로 mutable로 선언함
		mutable std::mutex _mutex;

		// 기다리고 있는 도중 Push()하면, 우선순위를 다시 가늠해야 하는데 이 때 사용되는 변수
		Event _event;
	};
}
