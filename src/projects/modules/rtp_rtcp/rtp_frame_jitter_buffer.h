#pragma once

#include "base/ovlibrary/ovlibrary.h"
#include "rtp_packet.h"
#include <unordered_map>

#define DEFAULT_VIDEO_MAX_BUFFERING_TIME_MS	100	 // 100ms

// RTP Packet Group by Frame
class RtpFrame
{
public:
	RtpFrame();
	bool InsertPacket(const std::shared_ptr<RtpPacket> &packet);
	bool IsCompleted();
	uint64_t GetElapsed();

	std::shared_ptr<RtpPacket> GetFirstRtpPacket();
	std::shared_ptr<RtpPacket> GetNextRtpPacket();

	uint32_t Timestamp(){return _timestamp;}
	size_t PacketCount(){return _packets.size();}

private:
	bool CheckCompleted();

	ov::StopWatch _stop_watch;

	uint32_t	_timestamp = 0;
	
	uint16_t	_curr_sequence_number = 0;
	uint16_t	_first_sequence_number = 0;
	uint16_t	_marker_sequence_number = 0;
	bool		_marked = 0;

	bool		_completed = false;

	// sequence number : RtpPacket
	std::unordered_map<uint16_t, std::shared_ptr<RtpPacket>> _packets;
};

// A jitter buffer for a media stream in the form that the frame is fragmented
// and the rtp marker bit indicates that it is the last fragment.
class RtpFrameJitterBuffer
{
public:
	bool InsertPacket(const std::shared_ptr<RtpPacket> &packet);
	bool HasAvailableFrame();
	std::shared_ptr<RtpFrame> PopAvailableFrame();
	
private:	
	void BurnOutExpiredFrames();

	uint32_t _buffer_size_ms = DEFAULT_VIDEO_MAX_BUFFERING_TIME_MS;
	// timestamp : RtpFrameInfo
	// it should be ordered, so use std::map
	std::map<uint32_t, std::shared_ptr<RtpFrame>> _rtp_frames;
};