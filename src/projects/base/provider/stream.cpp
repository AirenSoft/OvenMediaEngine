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
		double max_timestamp_ms = 0;
		for(const auto &item : _last_timestamp_map)
		{
			auto track_id = item.first;
			auto timestamp = item.second;
			auto track = GetTrack(track_id);
			if(track == nullptr)
			{
				return;
			}

			auto timestamp_ms = (timestamp * 1000) / track->GetTimeBase().GetTimescale();
			logtd("%d old timestamp : %f ms", track_id, timestamp_ms);

			max_timestamp_ms = std::max<double>(timestamp_ms, max_timestamp_ms);
		}

		for(const auto &item : _last_timestamp_map)
		{
			auto track_id = item.first;
			[[maybe_unused]] auto old_timestamp = item.second;
			auto track = GetTrack(track_id);

			// Since the stream is switched, initialize last_timestamp and base_timestamp to receive a new stream. However, some players do not allow the timestamp to decrease, so it is initialized based on the largest timestamp value among previous tracks. 
			// If the timestamp is incremented by 60 seconds (big jump), it seems that the player flushes the old stream and starts anew. (This is an experimental estimate. If 60 seconds are added to the timestamp, no video stuttering in the webrtc browser is observed.)
			auto adjust_timestamp = (max_timestamp_ms * track->GetTimeBase().GetTimescale() / 1000) + (60 * track->GetTimeBase().GetTimescale());

			// base_timestamp is the last timestamp value of the previous stream. Increase it based on this.
			// last_timestamp is a value that is updated every time a packet is received.
			_base_timestamp_map[track_id] = adjust_timestamp;
			_last_timestamp_map[track_id] = adjust_timestamp;

			logtd("Reset %d last timestamp : %lld => %lld", track_id, old_timestamp, _last_timestamp_map[track_id]);
		}

		_source_timestamp_map.clear();
	}

	// This keeps the pts value of the input track (only the start value<base_timestamp> is different), meaning that this value can be used for A/V sync.
	uint64_t Stream::AdjustTimestampByBase(uint32_t track_id, uint64_t timestamp, uint64_t max_timestamp)
	{
		uint64_t base_timestamp = 0;
		if(_base_timestamp_map.find(track_id) != _base_timestamp_map.end())
		{
			base_timestamp = _base_timestamp_map[track_id];
		}

		_last_timestamp_map[track_id] = base_timestamp + timestamp;
		return _last_timestamp_map[track_id];
	}

	uint64_t Stream::GetBaseTimestamp(uint32_t track_id)
	{
		uint64_t base_timestamp = 0;
		if(_base_timestamp_map.find(track_id) != _base_timestamp_map.end())
		{
			base_timestamp = _base_timestamp_map[track_id];
		}

		return base_timestamp;
	}

	// This is a method of generating a PTS with an increment value (delta) when it cannot be used as a PTS because the start value of the timestamp is random like the RTP timestamp.
	uint64_t Stream::AdjustTimestampByDelta(uint32_t track_id, uint64_t timestamp, uint64_t max_timestamp)
	{
		uint32_t curr_timestamp; 

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

	uint64_t Stream::GetDeltaTimestamp(uint32_t track_id, uint64_t timestamp, uint64_t max_timestamp)
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

		uint64_t delta = 0;

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