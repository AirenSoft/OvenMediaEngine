//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#include "segment_stream_interceptor.h"

#define OV_LOG_TAG "SegmentStream"

SegmentStreamInterceptor::~SegmentStreamInterceptor()
{
    auto it = _thread_checkers.begin();
    while(it != _thread_checkers.end())
    {
        if((*it)->thread->joinable())
            (*it)->thread->join();
    }

    _thread_checkers.clear();
}

//====================================================================================================
// IsInterceptorForRequest
//====================================================================================================
bool SegmentStreamInterceptor::IsInterceptorForRequest(const std::shared_ptr<const HttpRequest> &request, const std::shared_ptr<const HttpResponse> &response)
{
	logtd("Request Target : %s", request->GetRequestTarget().CStr());

	// Get Method 1.1 이상 체크
	if((request->GetMethod() != HttpMethod::Get) || (request->GetHttpVersionAsNumber() <= 1.0))
	{
		return false;
	}

	return true;
}

bool SegmentStreamInterceptor::OnHttpData(const std::shared_ptr<HttpRequest> &request, const std::shared_ptr<HttpResponse> &response, const std::shared_ptr<const ov::Data> &data)
{
    //temp
    // thread end check
   //logtd("Begin thread begin(%d)", _thread_checkers.size());
    auto it = _thread_checkers.begin();
    while(it != _thread_checkers.end())
    {
        if((*it)->is_closed)
        {
            if((*it)->thread->joinable())
                (*it)->thread->join();

            _thread_checkers.erase(it++);
        }
        else
            ++it;
    }
    //logtd("After thread check(%d)", _thread_checkers.size());

     auto thread_checker = std::make_shared<ThreadChecker>();

    // create thread
    std::shared_ptr<std::thread> thread( new std::thread(
            [this, request, response, data, thread_checker]()
            {
                HttpDefaultInterceptor::OnHttpData(request, response, data);

                thread_checker->is_closed = true;
            }
    ));
    thread_checker->thread = thread;

    // insert
    _thread_checkers.push_back(thread_checker);

	return true;
}