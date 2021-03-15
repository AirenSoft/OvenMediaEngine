#include "rtp_jitter_buffer.h"

#define OV_LOG_TAG "RtpJitterBuffer"

/************************************************************************
 * 								RTPFrame
 ***********************************************************************/

RtpFrame::RtpFrame()
{
	_stop_watch.Start();
}

std::shared_ptr<RtpPacket> RtpFrame::GetFirstRtpPacket()
{
	if(IsCompleted() == false)
	{
		return nullptr;
	}

	_curr_sequence_number = _first_sequence_number;

	auto it = _packets.find(_curr_sequence_number);
	if(it == _packets.end())
	{
		return nullptr;
	}

	return it->second;
}

std::shared_ptr<RtpPacket> RtpFrame::GetNextRtpPacket()
{
	if(IsCompleted() == false)
	{
		return nullptr;
	}

	_curr_sequence_number ++;

	auto it = _packets.find(_curr_sequence_number);
	if(it == _packets.end())
	{
		return nullptr;
	}

	return it->second;
}

bool RtpFrame::InsertPacket(const std::shared_ptr<RtpPacket> &packet)
{
	auto inserted = _packets.insert(std::pair<uint16_t, std::shared_ptr<RtpPacket>>(packet->SequenceNumber(), packet));
	if(inserted.second == false)
	{
		// duplicated packet
		return false;
	}

	// First packet
	if(_timestamp == 0 || packet->SequenceNumber() < _first_sequence_number)
	{
		_timestamp = packet->Timestamp();
		_first_sequence_number = packet->SequenceNumber();
	}

	if(packet->Marker())
	{
		_marked = true;
		_marker_sequence_number = packet->SequenceNumber();
	}
	
	// Check if frame is completed
	if(_marked == true)
	{
		CheckCompleted();
	}

	return true;
}

bool RtpFrame::IsCompleted()
{
	if(_completed == true)
	{
		return true;
	}

	if(_marked == true)
	{
		return CheckCompleted();
	}

	return false;
}

bool RtpFrame::CheckCompleted()
{
	// Already completed
	if(_completed == true)
	{
		return true;
	}

	if(_marked == false)
	{
		return false;
	}

	// Check number of packets
	uint16_t need_number_of_packets = 0;
	if(_marker_sequence_number >= _first_sequence_number)
	{
		need_number_of_packets = _marker_sequence_number - _first_sequence_number;
	}
	// Rounded
	else
	{
		// first ~ max, 0, 0 ~ marker
		need_number_of_packets = (std::numeric_limits<uint16_t>::max() - _first_sequence_number) + _marker_sequence_number + 1;
	}

	need_number_of_packets += 1;
	
	// Check if frame is valid
	if(need_number_of_packets == _packets.size())
	{
		_completed = true;
		return true;
	}

	return false;
}

uint64_t RtpFrame::GetElapsed()
{
	return _stop_watch.Elapsed();
}

/************************************************************************
 * 							Jitter Buffer
 ***********************************************************************/

bool RtpJitterBuffer::InsertPacket(const std::shared_ptr<RtpPacket> &packet)
{
	auto it = _rtp_frames.find(packet->Timestamp());
	std::shared_ptr<RtpFrame> frame;

	if(it == _rtp_frames.end())
	{
		logtd("Create frame buffer for timestamp %u", packet->Timestamp());
		// First packet of frame
		frame = std::make_shared<RtpFrame>();
		_rtp_frames[packet->Timestamp()] = frame;
	}
	else
	{
		frame = it->second;
	}

	frame->InsertPacket(packet);

	return true;
}

void RtpJitterBuffer::BurnOutExpiredFrames()
{
	uint64_t expired_time;
	bool last_item;

	auto it = _rtp_frames.begin();
	while(it != _rtp_frames.end())
	{
		last_item = false;
		auto next_it = std::next(it, 1);
		if(next_it == _rtp_frames.end())
		{
			// Current item is last
			last_item = true;
		}

		std::shared_ptr<RtpFrame> frame = it->second;
		std::shared_ptr<RtpFrame> next_frame = last_item ? nullptr : next_it->second;

		// If next frame is completed but current frame is not completed,
		// it is determined that the current frame cannot be recovered even after waiting for more.
		// Therefore, it quickly gives up the current frame and reduces latency.
		expired_time = _buffer_size_ms;
		if(last_item == false && next_frame->IsCompleted())
		{
			expired_time /= 2;
		}

		if(frame->GetElapsed() > expired_time)
		{
			logtd("Burn out frame - timestamp(%u) packets(%d)", frame->Timestamp(), frame->PacketCount());
			it = _rtp_frames.erase(it);
		}
		else
		{
			++it;
		}
	}
}

bool RtpJitterBuffer::HasAvailableFrame()
{
	BurnOutExpiredFrames();

	auto it = _rtp_frames.begin();
	if(it == _rtp_frames.end())
	{
		return false;
	}

	auto first_frame = it->second;
	return first_frame->IsCompleted();
}

std::shared_ptr<RtpFrame> RtpJitterBuffer::PopAvailableFrame()
{
	if(HasAvailableFrame() == false)
	{
		return nullptr;
	}

	auto it = _rtp_frames.begin();
	auto frame = it->second;
	
	logtd("Pop frame - timestamp(%u) packets(%d)", frame->Timestamp(), frame->PacketCount());

	// remove front frame
	_rtp_frames.erase(it);

	return frame;
}