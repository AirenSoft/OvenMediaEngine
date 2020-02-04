//
// Created by getroot on 20. 1. 16.
//

#include "monitoring_private.h"
#include "application_metrics.h"
#include "host_metrics.h"

namespace mon
{
    bool ApplicationMetrics::OnStreamCreated(const info::Stream &stream)
    {
        std::unique_lock<std::mutex> lock(_map_guard);
        
        // If already stream metrics is exist,
        if(_streams.find(stream.GetId()) != _streams.end())
        {
            return true;
        }

        auto stream_metrics = std::make_shared<StreamMetrics>(GetSharedPtr(), stream);
        if(stream_metrics == nullptr)
        {
            logte("Cannot create StreamMetrics (%s/%s)", GetName().CStr(), stream.GetName().CStr());
            return false;
        }

        _streams[stream.GetId()] = stream_metrics;

        logti("Create StreamMetrics(%s) for monitoring", stream.GetName().CStr());
        return true;
    }

    bool ApplicationMetrics::OnStreamDeleted(const info::Stream &stream)
    {
        std::unique_lock<std::mutex> lock(_map_guard);
        if(_streams.find(stream.GetId()) == _streams.end())
        {
            return false;
        }
        _streams.erase(stream.GetId());

        logti("Delete StreamMetrics(%s) for monitoring", stream.GetName().CStr());
        return true;
    }

    std::shared_ptr<StreamMetrics> ApplicationMetrics::GetStreamMetrics(const info::Stream &stream)
    {
        std::unique_lock<std::mutex> lock(_map_guard);
        if(_streams.find(stream.GetId()) == _streams.end())
        {
            return nullptr;
        }
        
        return _streams[stream.GetId()];
    }
}