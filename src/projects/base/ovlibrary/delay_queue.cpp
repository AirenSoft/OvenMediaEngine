//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "./delay_queue.h"
#include "./log.h"
#include "./ovlibrary_private.h"

#include <thread>
#include <unistd.h>

namespace ov
{
	DelayQueue::DelayQueue()
		: _index(0L),

		  _stop(true)
	{
	}

	DelayQueue::~DelayQueue()
	{
		Stop();
	}

	void DelayQueue::Push(const DelayQueueFunction &func, void *parameter, int after, bool repeat)
	{
		std::lock_guard<std::mutex> lock(_mutex);

		int64_t index = _index;
		_index++;

		logtd("Pushing new item: %p (after %d ms)", parameter, after);

		_queue.emplace(index, func, parameter, after, repeat);

		logtd("Notifying...");
		_event.SetEvent();
	}

	void DelayQueue::Push(const DelayQueueFunction &func, int after)
	{
		Push(func, nullptr, after, false);
	}

	ssize_t DelayQueue::GetCount() const
	{
		std::lock_guard<std::mutex> lock(_mutex);

		return _queue.size();
	}

	bool DelayQueue::Start()
	{
		if(_stop == false)
		{
			// 이미 실행 중
			return false;
		}

		_stop = false;
		_thread = std::thread(std::bind(&DelayQueue::DispatchThreadProc, this));

		return true;
	}

	bool DelayQueue::Stop()
	{
		if(_stop)
		{
			// 이미 중지됨
			return false;
		}

		_stop = true;
		_thread.join();

		return true;
	}

	void DelayQueue::DispatchThreadProc()
	{
		while(_stop == false)
		{
			if(_queue.empty())
			{
				// queue에 아무 것도 없음. 새로운 항목이 Push될 때까지 대기
				logtd("Queue is empty. Waiting for new item...");

				_event.Wait();

				logtd("An item is pushed. Processing...");
			}
			else
			{
				// queue에 들어 있는 첫 번째 항목 처리
				auto first_item = _queue.top();

				if(_event.Wait(first_item.time_point) == false)
				{
					// first_item.time_point 만큼 대기 할 때까지, 다른 항목이 Push() 되지 않았음
					bool cont = first_item.function(first_item.parameter);

					std::lock_guard<std::mutex> lock(_mutex);

					// 처리된 항목은 queue에서 제외
					_queue.pop();

					if(first_item.repeat && cont)
					{
						// repeat이 활성화 되어 있고 true를 반환했다면 다음 번 실행을 위해 다시 등록
						first_item.RecalculateTimePoint();

						_queue.push(first_item);
					}
				}
				else
				{
					// first_item.time_point 만큼 대기하던 도중, 다른 항목이 Push() 됨
					// 새로 Push()된 항목이 더 작은 time_point를 가리키고 있을 수 있으므로, 다시 계산해야 함
					logtd("Another item is pushed while waiting the condition");
				}
			}
		}
	}
}