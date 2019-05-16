//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include <string>
#include <queue>
#include "base/ovlibrary/ovlibrary.h"
#include "base/ovlibrary/semaphore.h"
#include "http_server/http_request.h"
#include "http_server/http_response.h"
struct SegmentWorkInfo
{
    SegmentWorkInfo(const std::shared_ptr<HttpRequest> &request_,
                    const std::shared_ptr<HttpResponse> &response_)
    {
        request = request_;
        response = response_;
    }
    std::shared_ptr<HttpRequest> request = nullptr;
    std::shared_ptr<HttpResponse> response = nullptr;
    int retry_count = 0;
};

using SegmentProcessHandler = std::function<bool(const std::shared_ptr<HttpRequest> &request,
                                            const std::shared_ptr<HttpResponse> &response,
                                            int retry_count,
                                            bool &is_retry)>;
//====================================================================================================
// SegmentWorker
//====================================================================================================
class SegmentWorker
{
public:
    SegmentWorker();
    ~SegmentWorker();

    bool Start(const SegmentProcessHandler &process_handler);
    bool Stop();

    bool AddWorkInfo(std::shared_ptr<SegmentWorkInfo> work_info);
    std::shared_ptr<SegmentWorkInfo> PopWorkInfo();

private:
    void WorkerThread();

private :
    std::queue<std::shared_ptr<SegmentWorkInfo>> _work_infos;
    std::mutex _work_info_guard;
    ov::Semaphore _queue_event;

    bool _stop_thread_flag;
    std::thread _worker_thread;

    SegmentProcessHandler _process_handler;
};

//====================================================================================================
// SegmentWorkerManager
//====================================================================================================
class SegmentWorkerManager
{
public :
    SegmentWorkerManager() = default;
    ~SegmentWorkerManager() = default;

public :
    bool Start(int worker_count, const SegmentProcessHandler &process_handler);
    bool Stop();
    bool AddWork(const std::shared_ptr<HttpRequest> &request,
                 const std::shared_ptr<HttpResponse> &response);

private:
    int _worker_count = 0;
    int _worker_index = 0;

    std::vector<std::shared_ptr<SegmentWorker>> _workers;
};