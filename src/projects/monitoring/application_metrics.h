//
// Created by getroot on 20. 1. 16.
//
#pragma once

#include "base/info/info.h"
#include "base/common_types.h"
#include "stream_metrics.h"

namespace mon
{
	class ApplicationMetrics : public info::Application
	{

	private:
		std::map<uint32_t, std::shared_ptr<StreamMetrics>> _streams;
	};
}