//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <atomic>
#include <shared_mutex>

#include "delay_queue.h"
#include "stop_watch.h"

namespace ov
{
	class BpsCalculator
	{
	public:
		BpsCalculator();

		void AddBits(int64_t bits);

		int64_t GetTotalBits() const;
		int64_t GetTotalElapsed() const;

		int64_t GetBps() const;

	protected:
		DelayQueue _timer{"BpsCalc"};
		StopWatch _stop_watch;

		std::shared_mutex _mutex;
		int64_t _bits[10]{0};
		int _bits_count = 0;

		// Cumulative number of bits of last second
		std::atomic<int64_t> _current_bits{0L};
		// Cumulative number of bits of last <OV_COUNT(bits)> seconds
		std::atomic<int64_t> _acc_bits{0L};
		// Total number of bits
		std::atomic<int64_t> _total_bits{0L};
	};
}  // namespace ov