//==============================================================================
//
//  Provider Base Class
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#include "stream.h"

#include "application.h"
#include "base/info/application.h"
#include "provider_private.h"
#include "base/provider/pull_provider/stream_props.h"
#include "base/provider/pull_provider/stream.h"


namespace pvd
{
	Stream::Stream(StreamSourceType source_type)
		: info::Stream(source_type),
		  _application(nullptr)
	{
	}

	Stream::Stream(const std::shared_ptr<pvd::Application> &application, StreamSourceType source_type)
		: info::Stream(*(std::static_pointer_cast<info::Application>(application)), source_type),
		  _application(application)
	{
	}

	Stream::Stream(const std::shared_ptr<pvd::Application> &application, info::stream_id_t stream_id, StreamSourceType source_type)
		: info::Stream((*std::static_pointer_cast<info::Application>(application)), stream_id, source_type),
		  _application(application)
	{
	}

	Stream::Stream(const std::shared_ptr<pvd::Application> &application, const info::Stream &stream_info)
		: info::Stream(stream_info),
		  _application(application)
	{
	}

	Stream::~Stream()
	{
	}

	bool Stream::Start()
	{
		logti("%s/%s(%u) has been started stream", GetApplicationName(), GetName().CStr(), GetId());

		UpdateReconnectTimeToBasetime();

		return true;
	}

	bool Stream::Stop()
	{
		if (GetState() == Stream::State::STOPPED)
		{
			return true;
		}

		logti("%s/%s(%u) has been stopped playing stream", GetApplicationName(), GetName().CStr(), GetId());
		ResetSourceStreamTimestamp();

		_state = State::STOPPED;

		return true;
	}

	bool Stream::Terminate()
	{
		_state = State::TERMINATED;
		return true;
	}

	// Consider the reconnection time and add it to the base timestamp
	void Stream::UpdateReconnectTimeToBasetime()
	{
		if (_last_pkt_received_time != std::chrono::time_point<std::chrono::system_clock>::min())
		{
			auto reconnection_time_us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - _last_pkt_received_time).count();

			logtd("Time taken to reconnect is %lld milliseconds. add to the basetime", reconnection_time_us/1000);

			for (const auto &[track_id, timestamp] : _base_timestamp_map)
			{
				_base_timestamp_map[track_id] = (timestamp + reconnection_time_us);
			}
		}
	}

	const char *Stream::GetApplicationTypeName()
	{
		if (GetApplication() == nullptr)
		{
			return "Unknown";
		}

		return GetApplication()->GetApplicationTypeName();
	}

	bool Stream::SendDataFrame(int64_t timestamp, const cmn::BitstreamFormat &format, const cmn::PacketType &packet_type, const std::shared_ptr<ov::Data> &frame, bool urgent)
	{
		if (frame == nullptr)
		{
			return false;
		}

		auto data_track = GetFirstTrackByType(cmn::MediaType::Data);
		if (data_track == nullptr)
		{
			logte("Data track is not found. %s/%s(%u)", GetApplicationName(), GetName().CStr(), GetId());
			return false;
		}

		auto event_message = std::make_shared<MediaPacket>(GetMsid(),
															cmn::MediaType::Data,
															data_track->GetId(),
															frame, 
															timestamp,
															timestamp,
															format,
															packet_type);
		event_message->SetHighPriority(urgent);

		return SendFrame(event_message);
	}

	bool Stream::SendEvent(const std::shared_ptr<MediaEvent> &event)
	{
		if (event == nullptr)
		{
			return false;
		}

		auto data_track = GetFirstTrackByType(cmn::MediaType::Data);
		if (data_track == nullptr)
		{
			logte("Data track is not found. %s/%s(%u)", GetApplicationName(), GetName().CStr(), GetId());
			return false;
		}

		event->SetMsid(GetMsid());
		event->SetTrackId(data_track->GetId());

		return SendFrame(event);
	}

	std::shared_ptr<ov::Url> Stream::GetRequestedUrl() const
	{
		return _requested_url;
	}

	void Stream::SetRequestedUrl(const std::shared_ptr<ov::Url> &requested_url)
	{
		_requested_url = requested_url;
	}

	std::shared_ptr<ov::Url> Stream::GetFinalUrl() const
	{
		return _final_url;
	}

	void Stream::SetFinalUrl(const std::shared_ptr<ov::Url> &final_url)
	{
		_final_url = final_url;
	}

	bool Stream::SendFrame(const std::shared_ptr<MediaPacket> &packet)
	{
		if (_application == nullptr)
		{
			return false;
		}

		if (packet->GetPacketType() == cmn::PacketType::Unknown)
		{
			logte("The packet type must be specified. %s/%s(%u)", GetApplicationName(), GetName().CStr(), GetId());
			return false;
		}

		if (packet->GetPacketType() != cmn::PacketType::OVT &&
			packet->GetBitstreamFormat() == cmn::BitstreamFormat::Unknown)
		{
			logte("The bitstream format must be specified. %s/%s(%u)", GetApplicationName(), GetName().CStr(), GetId());
			return false;
		}

		// Statistics
		MonitorInstance->IncreaseBytesIn(*GetSharedPtrAs<info::Stream>(), packet->GetData()->GetLength());

		_last_pkt_received_time = std::chrono::system_clock::now();

		return _application->SendFrame(GetSharedPtr(), packet);
	}

	bool Stream::SetState(State state)
	{
		// STOPPED state is only set by calling Stream::Stop()
		if (state == State::STOPPED)
		{
			return false;
		}

		_state = state;
		return true;
	}

	void Stream::ResetSourceStreamTimestamp()
	{
		
#if 0	
		// Set the last timestamp of the lowest value of all tracks
		// Since the first packet of video usually starts with a keyframe, this tends to discard the first keyframe.
		int64_t last_timestamp = std::numeric_limits<int64_t>::max();
		for (const auto &[track_id, timestamp] : _last_timestamp_map)
		{
			auto track = GetTrack(track_id);
			if (!track)
			{
				continue;
			}

			last_timestamp = std::min<int64_t>(timestamp, last_timestamp);
		}
#else
		// Set the last timestamp of the highest value of all tracks
		// In this algorithm, the timestamp of A or V jumps for synchronization.
		// But after testing with a variety of players, this is better.
		int64_t last_timestamp = std::numeric_limits<int64_t>::min();
		for (const auto &[track_id, timestamp] : _last_timestamp_map)
		{
			auto track = GetTrack(track_id);
			if (!track)
			{
				continue;
			}

			last_timestamp = std::max<int64_t>(timestamp, last_timestamp);
		}	
#endif

		// Update base timestamp using last received timestamp
		for (const auto &[track_id, timestamp] : _last_timestamp_map)
		{
			// base_timestamp is the last timestamp value of the previous stream. Increase it based on this.
			// last_timestamp is a value that is updated every time a packet is received.
			[[maybe_unused]]
			int64_t prev_base_timestamp = _base_timestamp_map[track_id];
			
			_base_timestamp_map[track_id] = last_timestamp;

			logtd("%s/%s(%u) Update base timestamp [%d] %lld => %lld, last_timestamp: %lld",
				  GetApplicationName(), GetName().CStr(), GetId(),
				  track_id, prev_base_timestamp, _base_timestamp_map[track_id], last_timestamp);
		}

		// Initialized start timestamp
		_start_timestamp = -1LL;

		_source_timestamp_map.clear();
	}

	void Stream::RegisterRtpClock(uint32_t track_id, double clock_rate)
	{
		_rtp_lip_sync_clock.RegisterRtpClock(track_id, clock_rate);
	}

	void Stream::UpdateSenderReportTimestamp(uint32_t track_id, uint32_t msw, uint32_t lsw, uint32_t timestamp)
	{
		_rtp_lip_sync_clock.UpdateSenderReportTime(track_id, msw, lsw, timestamp);
	}

	bool Stream::AdjustRtpTimestamp(uint32_t track_id, int64_t timestamp, int64_t max_timestamp, int64_t &adjusted_timestamp)
	{
		// Make decision timestamp calculation method	
		if (_rtp_timestamp_method == RtpTimestampCalculationMethod::UNDER_DECISION)
		{
			if (GetDirectionType() == DirectionType::PULL)
			{
				// If this stream is type of PullStream, check the property of IgnoreRtcpSRTimestamp.
				auto stream = std::static_pointer_cast<pvd::PullStream>(GetSharedPtr());
				if(stream != nullptr)
				{
					auto props = stream->GetProperties();
					if(props != nullptr)
					{
						if(props->IsRtcpSRTimestampIgnored())
						{
							_rtp_timestamp_method = RtpTimestampCalculationMethod::SINGLE_DELTA;
						}
					}
				}
			}

			if (_rtp_timestamp_method == RtpTimestampCalculationMethod::UNDER_DECISION)
			{
				if ((GetMediaTrackCount(cmn::MediaType::Video) + GetMediaTrackCount(cmn::MediaType::Audio)) == 1)
				{
					logti("Since this stream has a single track, it computes PTS alone without RTCP SR.");
					_rtp_timestamp_method = RtpTimestampCalculationMethod::SINGLE_DELTA;
				}
				else if (_rtp_lip_sync_clock.IsEnabled() == true)
				{
					logti("Since this stream has received an RTCP SR, it counts the PTS with the SR.");
					_rtp_timestamp_method = RtpTimestampCalculationMethod::WITH_RTCP_SR;
				}
				// If it exceeds 5 seconds, it is calculated independently without RTCP SR.
				else if (_rtp_lip_sync_clock.IsEnabled() == false && _first_rtp_received_time.Elapsed() > 5000)
				{
					logtw("Since the RTCP SR was not received within 5 seconds, the PTS is calculated for each track without RTCP SR. (Lip-Sync may be out of sync)");
					_rtp_timestamp_method = RtpTimestampCalculationMethod::SINGLE_DELTA;
				}
				else if (_rtp_lip_sync_clock.IsEnabled() == false && _first_rtp_received_time.Elapsed() <= 5000)
				{
					// Wait for RTCP SR for 5 seconds
					if (_first_rtp_received_time.IsStart() == false)
					{
						logtw("Wait for RTCP SR for 5 seconds before starting the stream.");
						_first_rtp_received_time.Start();
					}
					return false; 
				}
			}
		}

		if (_rtp_timestamp_method == RtpTimestampCalculationMethod::WITH_RTCP_SR)
		{
			auto pts_base = _rtp_lip_sync_clock.CalcPTS(track_id, timestamp);
			if (pts_base.has_value() == false)
			{
				return false;
			}

			int64_t pts = pts_base.value();
			adjusted_timestamp = AdjustTimestampByBase(track_id, pts, pts, max_timestamp);
		}
		else if (_rtp_timestamp_method == RtpTimestampCalculationMethod::SINGLE_DELTA)
		{
			adjusted_timestamp = AdjustTimestampByDelta(track_id, timestamp, max_timestamp);
		}
		else
		{
			return false;
		}

		return true;
	}

	// This keeps the pts value of the input track (only the start value<base_timestamp> is different), meaning that this value can be used for A/V sync.
	// returns adjusted PTS and parameter PTS and DTS are also adjusted.
	int64_t Stream::AdjustTimestampByBase(uint32_t track_id, int64_t &pts, int64_t &dts, int64_t max_timestamp, int64_t duration)
	{
		auto track = GetTrack(track_id);
		if (!track)
		{
			return -1LL;
		}

		double expr_tb2us = track->GetTimeBase().GetExpr() * 1000000;
		double expr_us2tb = track->GetTimeBase().GetTimescale() / 1000000;

		// 1. Get the start timestamp and base timebase of this stream.
		if (_start_timestamp == -1LL)
		{
			_start_timestamp = (int64_t)((double)dts * expr_tb2us);

			// Updated stream's first timestamp should be next timestamp of last timestamp + duration of previous stream.
			if (_last_duration_map.find(track_id) != _last_duration_map.end())
			{
				_start_timestamp -= (_last_duration_map[track_id] * expr_tb2us);
			}

			// for debugging
			logtd("[%s/%s(%d)] Get start timestamp of stream. track:%d, ts:%lld (%d/%d) (%lldus)", _application->GetVHostAppName().CStr(), GetName().CStr(), GetId(), track_id, dts, track->GetTimeBase().GetNum(), track->GetTimeBase().GetDen(), _start_timestamp);
		}
		int64_t start_timestamp_tb = (int64_t)((double)_start_timestamp * expr_us2tb);

		// 2. Get the base timestamp of the track
		int64_t base_timestamp_tb = 0;
		auto it = _base_timestamp_map.find(track_id);
		if (it != _base_timestamp_map.end())
		{
			base_timestamp_tb = (int64_t)((double)it->second * expr_us2tb);
		}

		// 3. Calculate PTS/DTS (base_timestamp + (pts - start_timestamp))
		int64_t final_pkt_pts_tb = base_timestamp_tb + (pts - start_timestamp_tb);
		int64_t final_pkt_dts_tb = base_timestamp_tb + (dts - start_timestamp_tb);

		// 4. Check wrap around and adjust PTS/DTS

		// Initialize wraparound count for PTS
		if (_wraparound_count_map[0].find(track_id) == _wraparound_count_map[0].end())
		{
			_wraparound_count_map[0][track_id] = 0;
		}

		// Initialize wraparound count for DTS
		if (_wraparound_count_map[1].find(track_id) == _wraparound_count_map[1].end())
		{
			_wraparound_count_map[1][track_id] = 0;
		}

		// For PTS

		// PTS is not sequential. Therefore, the PTS may wrap around and return again.
		if (_last_origin_ts_map[0].find(track_id) != _last_origin_ts_map[0].end())
		{
			// Check if wrap arounded or reverse wrap arounded
			auto last_origin_pts = _last_origin_ts_map[0][track_id];
			if (last_origin_pts - pts > max_timestamp / 2)
			{
				_wraparound_count_map[0][track_id]++;
				logti("[PTS] Wrap around detected. track:%d", track_id);
			}
			else if (pts - last_origin_pts > max_timestamp / 2)
			{
				if (_wraparound_count_map[0][track_id] > 0)
				{
					_wraparound_count_map[0][track_id]--;
					logti("[PTS] Reverse wrap around detected. It could be caused by b-frames. track:%d", track_id);
				}
			}
		}

		final_pkt_pts_tb += _wraparound_count_map[0][track_id] * max_timestamp;

		// For DTS
		if (_last_origin_ts_map[1].find(track_id) != _last_origin_ts_map[1].end())
		{
			auto last_origin_dts = _last_origin_ts_map[1][track_id];
			if (last_origin_dts - dts > max_timestamp / 2)
			{
				_wraparound_count_map[1][track_id]++;
				logti("[DTS] Wrap around detected. track:%d", track_id);
			}
		}

		final_pkt_dts_tb += _wraparound_count_map[1][track_id] * max_timestamp;
		
		// 5. Update last timestamp ( Managed in microseconds )
		_last_timestamp_map[track_id] = (int64_t)((double)final_pkt_dts_tb * expr_tb2us);

		_last_origin_ts_map[0][track_id] = pts;
		_last_origin_ts_map[1][track_id] = dts;

		_last_duration_map[track_id] = duration;

		pts = final_pkt_pts_tb;
		dts = final_pkt_dts_tb;

#if 0
		// for debugging
		logti("[%s/%20s(%d)] track:%d, pts:%8lld -> %8lld (%8lldus), dts:%8lld -> %8lld (%8lldus), tb:%d/%d / lasttime:%lld, basetime:%lld",
				_application->GetName().CStr(), GetName().CStr(), GetId(), track_id,
				pts, final_pkt_pts_tb, (int64_t)((double)final_pkt_pts_tb * expr_tb2us), 
				dts, final_pkt_dts_tb, (int64_t)((double)final_pkt_dts_tb * expr_tb2us),
				track->GetTimeBase().GetNum(), track->GetTimeBase().GetDen(),
				_last_timestamp_map[track_id], (int64_t)(base_timestamp_tb * expr_tb2us));
#endif

		return pts;
	}

	int64_t Stream::GetBaseTimestamp(uint32_t track_id)
	{
		auto track = GetTrack(track_id);
		if (track == nullptr)
		{
			return -1LL;
		}

		int64_t base_timestamp = 0;
		if (_base_timestamp_map.find(track_id) != _base_timestamp_map.end())
		{
			base_timestamp = _base_timestamp_map[track_id];
		}

		auto base_timestamp_tb = (base_timestamp * track->GetTimeBase().GetTimescale() / 1000000);

		return base_timestamp_tb;
	}

	// This is a method of generating a PTS with an increment value (delta) when it cannot be used as a PTS because the start value of the timestamp is random like the RTP timestamp.
	int64_t Stream::AdjustTimestampByDelta(uint32_t track_id, int64_t timestamp, int64_t max_timestamp)
	{
		int64_t curr_timestamp;

		if (_last_timestamp_map.find(track_id) == _last_timestamp_map.end())
		{
			curr_timestamp = 0;
		}
		else
		{
			curr_timestamp = _last_timestamp_map[track_id];
		}

		auto delta = GetDeltaTimestamp(track_id, timestamp, max_timestamp);
		curr_timestamp += delta;

		_last_timestamp_map[track_id] = curr_timestamp;

		return curr_timestamp;
	}

	int64_t Stream::GetDeltaTimestamp(uint32_t track_id, int64_t timestamp, int64_t max_timestamp)
	{
		auto track = GetTrack(track_id);

		// First timestamp
		if (_source_timestamp_map.find(track_id) == _source_timestamp_map.end())
		{
			logtd("New track timestamp(%u) : curr(%lld)", track_id, timestamp);
			_source_timestamp_map[track_id] = timestamp;

			// Start with zero
			return 0;
		}

		int64_t delta = 0;

		// Wrap around or change source
		if (timestamp < _source_timestamp_map[track_id])
		{
			// If the last timestamp exceeds 99.99%, it is judged to be wrapped around.
			if (_source_timestamp_map[track_id] > ((double)max_timestamp * 99.99) / 100)
			{
				logtd("Wrapped around(%u) : last(%lld) curr(%lld)", track_id, _source_timestamp_map[track_id], timestamp);
				delta = (max_timestamp - _source_timestamp_map[track_id]) + timestamp;
			}
			// Otherwise, the source might be changed. (restarted)
			else
			{
				logtd("Source changed(%u) : last(%lld) curr(%lld)", track_id, _source_timestamp_map[track_id], timestamp);
				delta = 0;
			}
		}
		else
		{
			delta = timestamp - _source_timestamp_map[track_id];
		}

		_source_timestamp_map[track_id] = timestamp;
		return delta;
	}

	// Increase MSID and notify the application of the stream update
	bool Stream::UpdateStream()
	{
		if (_application == nullptr)
		{
			return false;
		}

		if (_application->UpdateStream(GetSharedPtr()) == false)
		{
			return false;
		}

		ResetSourceStreamTimestamp();
		SetMsid(GetMsid() + 1);

		logti("%s/%s(%u) has been updated stream", GetApplicationName(), GetName().CStr(), GetId());
		logti("%s", GetInfoString().CStr());

		return true;
	}
}  // namespace pvd