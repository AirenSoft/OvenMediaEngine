#pragma once

#include "base/ovlibrary/ovlibrary.h"

#define VIDEO_LIP_SYNC_CLOCK	0
#define AUDIO_LIP_SYNC_CLOCK	1

enum class ClockType : uint8_t
{
	VIDEO = 0,
	AUDIO = 1,
	NUMBER_OF_TYPE = 2
};

class LipSyncClock
{
public:
	LipSyncClock(double audio_timebase, double video_timebase);

	uint32_t GetNextTimestamp(ClockType type, uint32_t rtp_timestamp);
	bool ApplySenderReportTime(ClockType type, uint32_t ntp_msw, uint32_t ntp_lsw, uint32_t rtcp_timestamp);

private:
	struct Clock
	{
		double		_timebase = 0;
		
		double		_last_pts = 0;
		uint64_t	_addend_timestamp = 0;
		uint64_t	_last_timestamp = 0;

		uint32_t	_last_received_rtp_timestamp = 0;
		uint64_t	_last_received_ntp_timestamp_ms = 0; // Calculated

		bool		_first_sr_avaliable = false;
		uint64_t	_first_sr_ntp_timestamp_ms = 0;
		uint32_t	_first_sr_timestamp = 0;

		double		_ntp_per_timestamp = 0.0;
	} _clock[static_cast<uint8_t>(ClockType::NUMBER_OF_TYPE)];

	Clock&	GetClock(ClockType type);
};