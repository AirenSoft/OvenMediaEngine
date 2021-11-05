#include "lip_sync_clock.h"
#include "base/ovlibrary/clock.h"
#define OV_LOG_TAG "LipSyncClock"

bool LipSyncClock::RegisterClock(uint32_t id, double timebase)
{
	auto clock = std::make_shared<Clock>();
	clock->_timebase = timebase;
	_clock_map.emplace(id, clock);

	return true;
}

std::shared_ptr<LipSyncClock::Clock> LipSyncClock::GetClock(uint32_t id)
{
	if(_clock_map.find(id) == _clock_map.end())
	{
		return nullptr;
	}
	return _clock_map[id];
}

std::optional<uint64_t> LipSyncClock::CalcPTS(uint32_t id, uint32_t rtp_timestamp)
{
	auto clock = GetClock(id);
	if(clock == nullptr || clock->_updated == false)
	{
		return {};
	}

	std::shared_lock<std::shared_mutex> lock(clock->_clock_lock);
	// The timestamp difference can be negative.
	return clock->_pts + ((int64_t)rtp_timestamp - (int64_t)clock->_rtcp_timestamp);
}

bool LipSyncClock::UpdateSenderReportTime(uint32_t id, uint32_t ntp_msw, uint32_t ntp_lsw, uint32_t rtcp_timestamp)
{
	auto clock = GetClock(id);
	if(clock == nullptr)
	{
		return false;
	}

	std::lock_guard<std::shared_mutex> lock(clock->_clock_lock);
	clock->_updated = true;
	clock->_rtcp_timestamp = rtcp_timestamp;
	clock->_pts = ov::Converter::NtpTsToSeconds(ntp_msw, ntp_lsw) / clock->_timebase;

	logtd("Update SR : id(%u) NTP(%u/%u) pts(%lld) timestamp(%u)", id, ntp_msw, ntp_lsw, clock->_pts, clock->_rtcp_timestamp);

	return true;
}