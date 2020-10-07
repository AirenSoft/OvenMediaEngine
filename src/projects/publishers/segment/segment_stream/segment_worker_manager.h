//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/ovlibrary/ovlibrary.h>
#include <base/ovlibrary/semaphore.h>
#include <modules/http_server/http_client.h>

#include <queue>
#include <string>
struct SegmentWorkInfo
{
	SegmentWorkInfo(const std::shared_ptr<HttpClient> &client, const ov::String &request_target, const ov::String &origin_url)
		: client(client),
		  request_target(request_target),
		  origin_url(origin_url)
	{
	}

	std::shared_ptr<HttpClient> client = nullptr;
	ov::String request_target;
	ov::String origin_url;
};

using SegmentProcessHandler = std::function<bool(const std::shared_ptr<HttpClient> &client,
												 const ov::String &request_target,
												 const ov::String &origin_url)>;
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

private:
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
public:
	SegmentWorkerManager() = default;
	~SegmentWorkerManager() = default;

public:
	bool Start(int worker_count, const SegmentProcessHandler &process_handler);
	bool Stop();
	bool AddWork(const std::shared_ptr<HttpClient> &response,
				 const ov::String &request_target,
				 const ov::String &origin_url);

private:
	int _worker_count = 0;
	int _worker_index = 0;

	std::vector<std::shared_ptr<SegmentWorker>> _workers;
};