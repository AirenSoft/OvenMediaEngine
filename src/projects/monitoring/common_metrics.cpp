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

    uint32_t CommonMetrics::GetUnusedTimeSec()
    {
        auto current = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::seconds>(current - GetLastUpdatedTime()).count();
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