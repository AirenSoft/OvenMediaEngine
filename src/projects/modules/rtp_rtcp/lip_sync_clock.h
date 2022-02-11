#pragma once

#include "base/ovlibrary/ovlibrary.h"

class LipSyncClock
{
public:
	LipSyncClock() = default;

	bool RegisterClock(uint32_t id, double timebase);

	std::optional<uint64_t> CalcPTS(uint32_t id, uint32_t rtp_timestamp);
	bool UpdateSenderReportTime(uint32_t id, uint32_t ntp_msw, uint32_t ntp_lsw, uint32_t rtcp_timestamp);

	bool IsEnabled() {return _enabled;}

private:
	struct Clock
	{
		std::shared_mutex _clock_lock;
		bool		_updated = false;
		double		_timebase = 0;
		uint32_t	_rtcp_timestamp = 0;
		uint64_t	_pts = 0;	// converted NTP timestamp to timebase timestamp
	};

	// Id, Clock
	std::map<uint32_t, std::shared_ptr<Clock>> _clock_map;

	bool _enabled = false;

	std::shared_ptr<Clock> GetClock(uint32_t id);
};