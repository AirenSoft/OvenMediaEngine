//
// Created by getroot on 20. 1. 16.
//

#include "monitoring_private.h"
#include "application_metrics.h"
#include "host_metrics.h"

namespace mon
{
    void ApplicationMetrics::ShowInfo(bool show_children)
    {
        //TODO(Getroot): Print detailed information of application
        ov::String out_str = ov::String::FormatString("\n[Application Info]\nid(%u), name(%s)\nCreated Time (%s)\n", 														
														GetId(), GetName().CStr(),
														ov::Converter::ToString(_created_time).CStr());
        logti("%s", out_str.CStr());

        CommonMetrics::ShowInfo();

        if(show_children)
        {
            for(auto &t : _streams)
            {
                auto stream = t.second;
                stream->ShowInfo();
            }
        }
    }

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

    void ApplicationMetrics::IncreaseBytesIn(uint64_t value)
    {
        // Forward value to HostMetrics to sum
		GetHostMetrics()->IncreaseBytesIn(value);
		CommonMetrics::IncreaseBytesIn(value);
    }

    void ApplicationMetrics::IncreaseBytesOut(PublisherType type, uint64_t value)
    {
        // Forward value to HostMetrics to sum
		GetHostMetrics()->IncreaseBytesOut(type, value);
		CommonMetrics::IncreaseBytesOut(type, value);
    }

    void ApplicationMetrics::OnSessionConnected(PublisherType type)
    {
         // Forward value to HostMetrics to sum
		GetHostMetrics()->OnSessionConnected(type);
		CommonMetrics::OnSessionConnected(type);
    }

    void ApplicationMetrics::OnSessionDisconnected(PublisherType type)
    {
        // Forward value to HostMetrics to sum
		GetHostMetrics()->OnSessionDisconnected(type);
		CommonMetrics::OnSessionDisconnected(type);
    }
}