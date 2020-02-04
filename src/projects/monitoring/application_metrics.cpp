//
// Created by getroot on 20. 1. 16.
//

#include "monitoring_private.h"
#include "application_metrics.h"
#include "host_metrics.h"

namespace mon
{
    bool ApplicationMetrics::OnStreamCreated(const info::StreamInfo &stream_info)
    {
        auto stream_metrics = std::make_shared<StreamMetrics>(GetSharedPtr(), stream_info);
        if(stream_metrics == nullptr)
        {
            logte("Cannot create StreamMetrics (%s/%s)", GetName().CStr(), stream_info.GetName().CStr());
            return false;
        }

        std::unique_lock<std::mutex> lock(_map_guard);
        _streams[stream_info.GetId()] = stream_metrics;

        logti("Create StreamMetrics(%s) for monitoring", stream_info.GetName().CStr());
        return true;
    }

    bool ApplicationMetrics::OnStreamDeleted(const info::StreamInfo &stream_info)
    {
        std::unique_lock<std::mutex> lock(_map_guard);
        if(_streams.find(stream_info.GetId()) == _streams.end())
        {
            return false;
        }
        _streams.erase(stream_info.GetId());

        logti("Delete StreamMetrics(%s) for monitoring", stream_info.GetName().CStr());
        return true;
    }

    std::shared_ptr<StreamMetrics> ApplicationMetrics::GetStreamMetrics(const info::StreamInfo &stream_info)
    {
        std::unique_lock<std::mutex> lock(_map_guard);
        if(_streams.find(stream_info.GetId()) == _streams.end())
        {
            return nullptr;
        }
        
        return _streams[stream_info.GetId()];
    }
}