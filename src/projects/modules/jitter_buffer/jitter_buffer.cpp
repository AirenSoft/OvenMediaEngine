#include "jitter_buffer.h"

JitterBuffer::JitterBuffer(uint32_t track_id, uint64_t timebase)
	: _media_packet_queue("WebRTCJitterBuffer", 1000)
{
	_timebase = timebase;
	_track_id = track_id;
}

int64_t JitterBuffer::GetTimebase()
{
	return _timebase;
}

// JitterBuffer timebase is 1/1,000,000
int64_t JitterBuffer::GetNextPtsUsec()
{
	auto data = _media_packet_queue.Front(0);
	if(data.has_value() == false)
	{
		return -1;
	}

	auto origin_pts = data.value()->GetPts();
	auto next_pts = ((double)origin_pts / (double)_timebase) * JITTER_BUFFER_TIMEBASE;

	return next_pts;
}

bool JitterBuffer::PushMediaPacket(const std::shared_ptr<MediaPacket> &media_packet)
{
	_media_packet_queue.Enqueue(media_packet);
	return true;
}

std::shared_ptr<MediaPacket> JitterBuffer::PopNextMediaPacket()
{
	auto data = _media_packet_queue.Dequeue(0);
	if(data.has_value())
	{
		return data.value();
	}

	return nullptr;
}

bool JitterBuffer::IsEmpty()
{
	return _media_packet_queue.IsEmpty();
}

bool JitterBufferDelay::CreateJitterBuffer(uint32_t track_id, int64_t timebase)
{
	_media_packet_queue_map.emplace(track_id, std::make_shared<JitterBuffer>(track_id, timebase));
	return true;
}

bool JitterBufferDelay::PushMediaPacket(const std::shared_ptr<MediaPacket> &media_packet)
{
	auto it = _media_packet_queue_map.find(media_packet->GetTrackId());
	if(it == _media_packet_queue_map.end())
	{
		return false;
	}

	return it->second->PushMediaPacket(media_packet);
}

std::shared_ptr<MediaPacket> JitterBufferDelay::PopNextMediaPacket()
{
	if(DoesEveryBufferHavePackets() == false)
	{
		return nullptr;
	}

	// Pop lowest PTS first
	std::shared_ptr<JitterBuffer> next_jitter_buffer = nullptr;
	for(const auto &item : _media_packet_queue_map)
	{
		auto buffer = item.second;

		//logc("DEBUG", "Timebase : %u Next PTS : %u", buffer->GetTimebase(), buffer->GetNextPtsUsec());
		if(next_jitter_buffer == nullptr || buffer->GetNextPtsUsec() < next_jitter_buffer->GetNextPtsUsec())
		{
			next_jitter_buffer = buffer;
		}
	}

	return next_jitter_buffer->PopNextMediaPacket();
}

bool JitterBufferDelay::DoesEveryBufferHavePackets()
{
	if(_media_packet_queue_map.empty())
	{
		return false;
	}

	for(const auto &item : _media_packet_queue_map)
	{
		auto buffer = item.second;
		if(buffer->IsEmpty())
		{
			return false;
		}
	}

	return true;
}