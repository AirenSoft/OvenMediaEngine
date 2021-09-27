//==============================================================================
//
//  OvenMediaEngine
//
//  Copyright (c) 2021 Getroot. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/ovlibrary/ovlibrary.h>
#include <base/mediarouter/media_buffer.h>

#define JITTER_BUFFER_TIMEBASE	1000000.0 // microseconds
class JitterBuffer
{
public:
	JitterBuffer(uint32_t track_id, uint64_t timebase);

	// JitterBuffer timebase is 1/1,000,000
	int64_t GetTimebase();
	int64_t GetNextPtsUsec();
	bool PushMediaPacket(const std::shared_ptr<MediaPacket> &media_packet);
	std::shared_ptr<MediaPacket> PopNextMediaPacket();
	bool IsEmpty();
private:
	uint32_t _track_id = 0;
	int64_t _timebase = 0;
	int64_t _next_pts = 0;
	ov::Queue<std::shared_ptr<MediaPacket>>	_media_packet_queue;
};

// For Lip Sync
class JitterBufferDelay
{
public:
	bool CreateJitterBuffer(uint32_t track_id, int64_t timebase);
	bool PushMediaPacket(const std::shared_ptr<MediaPacket> &media_packet);
	std::shared_ptr<MediaPacket> PopNextMediaPacket();

private:
	bool DoesEveryBufferHavePackets();
	std::map<uint32_t, std::shared_ptr<JitterBuffer>>	_media_packet_queue_map;
};