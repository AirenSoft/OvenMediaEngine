//
// Created by getroot on 20. 1. 31.
//
#include "common_metrics.h"

namespace mon
{
    CommonMetrics::CommonMetrics()
    {
        _created_time = std::chrono::system_clock::now();
        UpdateDate();
    }

    const std::chrono::system_clock::time_point& CommonMetrics::GetCreatedTime()
    {
        return _created_time;
    }

    const std::chrono::system_clock::time_point& CommonMetrics::GetLastUpdatedTime()
    {
        return _last_updated_time;
    }

    // Renew last updated time
    void CommonMetrics::UpdateDate()
    {
        _last_updated_time = std::chrono::system_clock::now();
    }
}