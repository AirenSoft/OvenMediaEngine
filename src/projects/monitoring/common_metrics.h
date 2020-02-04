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
	protected:
		CommonMetrics();
		~CommonMetrics() = default;

		std::chrono::system_clock::time_point GetCreatedTime();
		std::chrono::system_clock::time_point GetLastUpdatedTime();

		// Renew last updated time
		void UpdateDate();

		std::chrono::system_clock::time_point _created_time;
		std::chrono::system_clock::time_point _last_updated_time;
	};
}  // namespace mon