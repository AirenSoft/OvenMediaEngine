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

namespace pvd
{
	Stream::Stream(StreamSourceType source_type)
		:info::Stream(source_type),
		_application(nullptr)
	{
	}

	Stream::Stream(const std::shared_ptr<pvd::Application> &application, StreamSourceType source_type)
		:info::Stream(*(std::static_pointer_cast<info::Application>(application)), source_type),
		_application(application)
	{
	}

	Stream::Stream(const std::shared_ptr<pvd::Application> &application, info::stream_id_t stream_id, StreamSourceType source_type)
		:info::Stream((*std::static_pointer_cast<info::Application>(application)), stream_id, source_type),
		_application(application)
	{
	}

	Stream::Stream(const std::shared_ptr<pvd::Application> &application, const info::Stream &stream_info)
		:info::Stream(stream_info),
		_application(application)
	{
	}

	Stream::~Stream()
	{

	}

	bool Stream::Start() 
	{
		logti("%s/%s(%u) has been started stream", GetApplicationName(), GetName().CStr(), GetId());
		return true;
	}
	
	bool Stream::Stop() 
	{
		if(GetState() == Stream::State::STOPPED)
		{
			return true;
		}

		logti("%s/%s(%u) has been stopped playing stream", GetApplicationName(), GetName().CStr(), GetId());
		ResetSourceStreamTimestamp();

		_state = State::STOPPED;
		return true;
	}

	const char* Stream::GetApplicationTypeName()
	{
		if(GetApplication() == nullptr)
		{
			return "Unknown";
		}

		return GetApplication()->GetApplicationTypeName();
	}

	bool Stream::SendFrame(const std::shared_ptr<MediaPacket> &packet)
	{
		if(_application == nullptr)
		{
			return false;
		}

		if(packet->GetPacketType() == cmn::PacketType::Unknown)
		{
			logte("The packet type must be specified. %s/%s(%u)", GetApplicationName(), GetName().CStr(), GetId());
			return false;
		}

		if(packet->GetPacketType() != cmn::PacketType::OVT && 
			packet->GetBitstreamFormat() == cmn::BitstreamFormat::Unknown)
		{
			logte("The bitstream format must be specified. %s/%s(%u)", GetApplicationName(), GetName().CStr(), GetId());
			return false;
		}

		// Statistics
		MonitorInstance->IncreaseBytesIn(*GetSharedPtrAs<info::Stream>(), packet->GetData()->GetLength());

		return _application->SendFrame(GetSharedPtr(), packet);
	}

	bool Stream::SetState(State state)
	{
		// STOPPED state is only set by calling Stream::Stop() 
		if(state == State::STOPPED)
		{
			return false;
		}
		
		_state = state;
		return true;
	}

	void Stream::ResetSourceStreamTimestamp()
	{
		// _timestamp_map is the timetamp for this stream, _last_timestamp_map is the timestamp for source timestamp
		// reset last timestamp map
		double max_timestamp_us = 0;
		for (const auto &item : _last_timestamp_map)
		{
			auto track_id = item.first;
			auto timestamp_us = item.second;
			auto track = GetTrack(track_id);
			if (track == nullptr)
			{
				return;
			}

			// auto timestamp_ms = (int64_t)((double)timestamp * (double)track->GetTimeBase().GetExpr() * 1000000);

			max_timestamp_us = std::max<double>(timestamp_us, max_timestamp_us);
		}

		logtd("%s/%s(%u) Find maximum timestamp: %f ms", GetApplicationName(), GetName().CStr(), GetId(), max_timestamp_us);

		for (const auto &item : _last_timestamp_map)
		{
			auto track_id = item.first;
			[[maybe_unused]] auto old_timestamp = item.second;
			auto track = GetTrack(track_id);

			// base_timestamp is the last timestamp value of the previous stream. Increase it based on this.
			// last_timestamp is a value that is updated every time a packet is received.
			_base_timestamp_map[track_id] = max_timestamp_us;
			_last_timestamp_map[track_id] = max_timestamp_us;

			logtw("%s/%s(%u) Reset %d last timestamp : %lld => %lld (%d/%d)", 
			GetApplicationName(), GetName().CStr(), GetId(), 
			track_id, old_timestamp, _last_timestamp_map[track_id], track->GetTimeBase().GetNum(), track->GetTimeBase().GetDen());
		}

		// Initialzed start timestamp
		_start_timestamp = -1LL;

		_source_timestamp_map.clear();
	}

	// This keeps the pts value of the input track (only the start value<base_timestamp> is different), meaning that this value can be used for A/V sync.
	int64_t Stream::AdjustTimestampByBase(uint32_t track_id, int64_t timestamp, int64_t max_timestamp)
	{
		auto track = GetTrack(track_id);
		if (track == nullptr)
		{
			return -1LL;			
		}

		double expr_tb2us = (track->GetTimeBase().GetExpr() * 1000000);
		double expr_us2tb = (track->GetTimeBase().GetTimescale() / 1000000);


		// 1. Set the start imestamp value of this stream.
		// * Managed in microseconds
		if (_start_timestamp == -1LL)
		{
			_start_timestamp = timestamp * expr_tb2us;
			logtw("Set Start timestamp is %lld(%lld us)", timestamp, _start_timestamp);
		}


		// 2. Zero based packet timestamp
		// - Convert start timestamp to timbase based timesatmp
		int64_t start_timestamp_tb = (int64_t)((double)_start_timestamp * expr_us2tb);
		// - Zerostart timestamp = source timesatmp - start timestamp
		int64_t zerostart_pkt_tmiestamp_tb = timestamp - (int64_t)start_timestamp_tb;


		// 3. Calculate the final timestamp
		// - Convert base timestamp to timbase based timesatmp
		int64_t base_timestamp_tb = 0;
		if (_base_timestamp_map.find(track_id) != _base_timestamp_map.end())
		{
			int64_t base_timestamp_us = _base_timestamp_map[track_id];
			base_timestamp_tb = (int64_t)((double)base_timestamp_us * expr_us2tb);
		}
		// - Fianl timestamp = base timesatmp - zerostart timestamp
		int64_t final_pkt_timestamp_tb = base_timestamp_tb + zerostart_pkt_tmiestamp_tb;


		// 4. Update last timestamp
		// * Managed in microseconds.
		_last_timestamp_map[track_id] = (int64_t)((double)final_pkt_timestamp_tb * expr_tb2us);

		return final_pkt_timestamp_tb;
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

		auto base_timestamp_tb =  (base_timestamp * track->GetTimeBase().GetTimescale() / 1000000);


		return base_timestamp_tb;
	}

	// This is a method of generating a PTS with an increment value (delta) when it cannot be used as a PTS because the start value of the timestamp is random like the RTP timestamp.
	int64_t Stream::AdjustTimestampByDelta(uint32_t track_id, int64_t timestamp, int64_t max_timestamp)
	{
		int64_t curr_timestamp; 

		if(_last_timestamp_map.find(track_id) == _last_timestamp_map.end())
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
		if(_source_timestamp_map.find(track_id) == _source_timestamp_map.end())
		{
			logtd("New track timestamp(%u) : curr(%lld)", track_id, timestamp);
			_source_timestamp_map[track_id] = timestamp;

			// Start with zero
			return 0;
		}

		int64_t delta = 0;

		// Wrap around or change source
		if(timestamp < _source_timestamp_map[track_id])
		{
			// If the last timestamp exceeds 99.99%, it is judged to be wrapped around.
			if(_source_timestamp_map[track_id] > ((double)max_timestamp * 99.99) / 100)
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
}