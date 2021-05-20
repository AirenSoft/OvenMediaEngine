#include "lip_sync_clock.h"

#include "base/ovlibrary/clock.h"

#define OV_LOG_TAG "LipSyncClock"

LipSyncClock::LipSyncClock(double audio_timebase, double video_timebase)
{
	GetClock(ClockType::AUDIO)._timebase = audio_timebase;
	GetClock(ClockType::VIDEO)._timebase = video_timebase;
}

LipSyncClock::Clock& LipSyncClock::GetClock(ClockType type)
{
	return _clock[static_cast<uint8_t>(type)];
}

uint32_t LipSyncClock::GetNextTimestamp(ClockType type, uint32_t rtp_timestamp)
{
	auto &clock = GetClock(type);

	// Rolling
	uint32_t delta = 0;
	
	if(clock._last_received_rtp_timestamp == 0)
	{
		delta = 0;
	}
	else 
	{
		delta = rtp_timestamp > clock._last_received_rtp_timestamp ? 
									(rtp_timestamp - clock._last_received_rtp_timestamp) :
								(std::numeric_limits<uint32_t>::max() - clock._last_received_rtp_timestamp + rtp_timestamp);
	}

	// Zero based timestamp
	clock._last_timestamp += delta;
	clock._last_received_rtp_timestamp = rtp_timestamp;

	// Caluate PTS using timebase
	clock._last_pts = static_cast<double>(clock._last_timestamp) * clock._timebase;

	if(clock._ntp_per_timestamp != 0)
	{
		clock._last_received_ntp_timestamp_ms = clock._first_sr_ntp_timestamp_ms + (static_cast<double>(rtp_timestamp - clock._first_sr_timestamp) * clock._ntp_per_timestamp);
		
		// Sync Video timestamp to audio
		if(type == ClockType::VIDEO)
		{
			auto audio_clock = GetClock(ClockType::AUDIO);
			if(clock._ntp_per_timestamp != 0 && audio_clock._ntp_per_timestamp != 0 &&
				clock._last_received_ntp_timestamp_ms != 0 && audio_clock._last_received_ntp_timestamp_ms != 0)
			{
				int64_t av_ntp_time_delta_ms = clock._last_received_ntp_timestamp_ms - audio_clock._last_received_ntp_timestamp_ms;
				double av_pts_delta_ms = clock._last_pts - audio_clock._last_pts;
				double av_gap = static_cast<double>(av_ntp_time_delta_ms) - av_pts_delta_ms;
				
				double calibrated_pts = 0.0;

				if(std::abs(av_gap) > 50.0)
				{
					// Adjust Video PTS
					calibrated_pts = (audio_clock._last_pts * static_cast<double>(clock._last_received_ntp_timestamp_ms)) / static_cast<double>(audio_clock._last_received_ntp_timestamp_ms);

					// Adjust Video Timestamp
					clock._addend_timestamp = (calibrated_pts / clock._timebase) - clock._last_timestamp;
					clock._last_timestamp = (calibrated_pts / clock._timebase);
					clock._last_pts = calibrated_pts;

					logti("Video timestamps have been adjusted for lip sync. (Video timestamp %llu)", clock._last_timestamp);
				}

				logti("Last Received Video NTP Timestamp(%llu) Last Received Audio NTP Timestamp(%llu) NTP Delta(%lld) Last Video PTS(%f) Last Audio PTS(%f) PTS Delta(%f) Calibrated Video PTS(%f) AV Gap(%f) Calibrated Video Timestamp(%u)", 
								clock._last_received_ntp_timestamp_ms, audio_clock._last_received_ntp_timestamp_ms, av_ntp_time_delta_ms, 
								clock._last_pts, audio_clock._last_pts, av_pts_delta_ms, calibrated_pts, av_gap, clock._last_timestamp);

			}
		}
	}

	return clock._last_timestamp;
}

bool LipSyncClock::ApplySenderReportTime(ClockType type, uint32_t ntp_msw, uint32_t ntp_lsw, uint32_t rtcp_timestamp)
{
	auto &clock = GetClock(type);

	// 1970 1 1
	ntp_msw -= 2208988800;

	// Now, doesn't update ntp_per_timestamp so it uses only two SR packets
	if(clock._ntp_per_timestamp != 0)
	{
		return true;
	}

	uint64_t ntp_timestamp_ms = (static_cast<uint64_t>(ntp_msw) * 1000) + (static_cast<uint64_t>(ntp_lsw) / static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()) * 1000);

	if(clock._first_sr_avaliable == false)
	{
		clock._first_sr_avaliable = true;
		clock._first_sr_ntp_timestamp_ms = ntp_timestamp_ms;
		clock._first_sr_timestamp = rtcp_timestamp;

		logtd("First SR : msw(%u) lsw(%u) ntp_ms(%llu) timestamp(%u)", ntp_msw, ntp_lsw, ntp_timestamp_ms, rtcp_timestamp);

		return true;
	}

	// Rolling
	uint32_t rtcp_timetamp_delta = rtcp_timestamp > clock._first_sr_timestamp ? 
										(rtcp_timestamp - clock._first_sr_timestamp) :
										(std::numeric_limits<uint32_t>::max() - clock._first_sr_timestamp + rtcp_timestamp);

	clock._ntp_per_timestamp = static_cast<double>(ntp_timestamp_ms - clock._first_sr_ntp_timestamp_ms) / static_cast<double>(rtcp_timetamp_delta);

	logtd("Second SR : msw(%u) lsw(%u) ntp_ms(%llu) timestamp(%u) _ntp_per_timestamp(%f)", ntp_msw, ntp_lsw, ntp_timestamp_ms, rtcp_timestamp, clock._ntp_per_timestamp);

	return true;
}