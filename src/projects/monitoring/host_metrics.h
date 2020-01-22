//
// Created by getroot on 20. 1. 16.
//

#pragma once

#include "base/common_types.h"
#include "application_metrics.h"

namespace mon
{
	class HostMetrics
	{

	private:
		std::map<uint32_t, std::shared_ptr<ApplicationMetrics>> _applications;
	};
}