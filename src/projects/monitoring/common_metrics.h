//
// Created by getroot on 20. 1. 31.
//

#pragma once

#include "base/common_types.h"
#include "base/info/info.h"
#include "base/info/stream.h"

namespace mon
{
	class CommonMetrics
	{
	public:
		uint32_t GetUnusedTimeSec();
		const std::chrono::system_clock::time_point& GetLastUpdatedTime();
		
	protected:
		CommonMetrics();
		~CommonMetrics() = default;

		// Renew last updated time
		void UpdateDate();

		std::chrono::system_clock::time_point _created_time;
		std::chrono::system_clock::time_point _last_updated_time;
	};
}  // namespace mon