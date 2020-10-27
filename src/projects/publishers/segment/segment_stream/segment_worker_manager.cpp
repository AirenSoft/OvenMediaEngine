#include "segment_worker_manager.h"
#include "segment_stream_private.h"

//====================================================================================================
// SegmentWorker constructorc
//====================================================================================================
SegmentWorker::SegmentWorker()
{
	_stop_thread_flag = true;
}

//====================================================================================================
// SegmentWorker destructor
//====================================================================================================
SegmentWorker::~SegmentWorker()
{
	Stop();
}

//====================================================================================================
// SegmentWorker start
//====================================================================================================
bool SegmentWorker::Start(const SegmentProcessHandler &process_handler)
{
	if (!_stop_thread_flag)
	{
		return true;
	}

	_process_handler = process_handler;

	_stop_thread_flag = false;
	_worker_thread = std::thread(&SegmentWorker::WorkerThread, this);

	return true;
}

//====================================================================================================
// SegmentWorker stop
//====================================================================================================
bool SegmentWorker::Stop()
{
	if (_stop_thread_flag)
	{
		return true;
	}

	_stop_thread_flag = true;
	// Generate Event
	_queue_event.Notify();
	if(_worker_thread.joinable())
	{
		_worker_thread.join();
	}

	return true;
}

//====================================================================================================
// add work info
//====================================================================================================
bool SegmentWorker::AddWorkInfo(std::shared_ptr<SegmentWorkInfo> work_info)
{
	std::unique_lock<std::mutex> lock(_work_info_guard);
	_work_infos.push(work_info);

	_queue_event.Notify();

	return true;
}

//====================================================================================================
// pop work info
//====================================================================================================
std::shared_ptr<SegmentWorkInfo> SegmentWorker::PopWorkInfo()
{
	std::unique_lock<std::mutex> lock(_work_info_guard);

	if (_work_infos.empty())
		return nullptr;

	auto work_info = _work_infos.front();
	_work_infos.pop();

	return work_info;
}

//====================================================================================================
// WokrThread main loop
//====================================================================================================
void SegmentWorker::WorkerThread()
{
	while (!_stop_thread_flag)
	{
		// quequ event wait
		_queue_event.Wait();

		auto work_info = PopWorkInfo();

		if (work_info == nullptr)
		{
			if (_stop_thread_flag == false)
			{
				// It's a problem if there's nothing in the queue despite the event
				OV_ASSERT2(false);
			}

			continue;
		}

		if (_process_handler(work_info->client, work_info->request_target, work_info->origin_url) == false)
		{
			logte("Segment process handler fail - target(%s)", work_info->request_target.CStr());
		}

		//        logtd("Segment process handler - target(%s)", work_info->request_target.CStr());
	}
}

//====================================================================================================
// Start
//====================================================================================================
bool SegmentWorkerManager::Start(int worker_count, const SegmentProcessHandler &process_handler)
{
	if (worker_count <= 0)
		return false;

	// Create WorkerThread
	for (int index = 0; index < worker_count; index++)
	{
		auto worker = std::make_shared<SegmentWorker>();
		worker->Start(process_handler);
		_workers.push_back(worker);
	}

	_worker_count = worker_count;

	return true;
}

//====================================================================================================
// Stop
//====================================================================================================
bool SegmentWorkerManager::Stop()
{
	for (const auto &worker : _workers)
	{
		worker->Stop();
	}

	return true;
}

//====================================================================================================
// Worker Add
//====================================================================================================
#define MAX_WORKER_INDEX 100000000
bool SegmentWorkerManager::AddWork(const std::shared_ptr<HttpClient> &response,
								   const ov::String &request_target,
								   const ov::String &origin_url)
{
	auto work_info = std::make_shared<SegmentWorkInfo>(response, request_target, origin_url);

	// insert thread
	_workers[(_worker_index % _worker_count)]->AddWorkInfo(work_info);

	if (_worker_index < MAX_WORKER_INDEX)
		_worker_index++;
	else
		_worker_index = 0;

	return true;
}
