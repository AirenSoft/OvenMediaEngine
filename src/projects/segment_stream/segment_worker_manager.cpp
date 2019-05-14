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
    if(!_stop_thread_flag)
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
    if(_stop_thread_flag)
    {
        return true;
    }

    _stop_thread_flag = true;
    // Generate Event
    _queue_event.Notify();
    _worker_thread.join();

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

    if(_work_infos.empty())
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
    while(!_stop_thread_flag)
    {
        // quequ event wait
        _queue_event.Wait();

        auto work_info = PopWorkInfo();

        if(work_info == nullptr)
        {
            continue;
        }

        bool is_retry = false;// in/out value

        _process_handler(work_info->request, work_info->response, work_info->retry_count, is_retry);

//        logtd("Segment Worker Process : url(%s) retry(%d:%d) result(%d)",
//              work_info->request->GetRequestTarget().CStr(),
//              is_retry,
//              work_info->retry_count,
//              result);

        if(is_retry)
        {
            // retry work input;
            work_info->retry_count++;
            AddWorkInfo(work_info);
        }
    }
}

//====================================================================================================
// Start
//====================================================================================================
bool SegmentWorkerManager::Start(int worker_count, const SegmentProcessHandler &process_handler)
{
    if(worker_count <= 0)
        return false;

    _worker_count = worker_count;

    // Create WorkerThread
    for(int index = 0; index < _worker_count ; index++)
    {
        auto worker = std::make_shared<SegmentWorker>();
        worker->Start(process_handler);
        _workers.push_back(worker);
    }

    return true;
}

//====================================================================================================
// Stop
//====================================================================================================
bool SegmentWorkerManager::Stop()
{
    for(const auto &worker : _workers)
        worker->Stop();

    return true;
}

//====================================================================================================
// Worker Add
//====================================================================================================
#define MAX_WORKER_INDEX 100000000
bool SegmentWorkerManager::AddWork(const std::shared_ptr<HttpRequest> &request,
                                   const std::shared_ptr<HttpResponse> &response)
{
    auto work_info = std::make_shared<SegmentWorkInfo>(request, response);

    // insert thread
    _workers[(_worker_index % _worker_count)]->AddWorkInfo(work_info);

    if(_worker_index < MAX_WORKER_INDEX)
        _worker_index++;
    else
        _worker_index = 0;

    return true;
}
