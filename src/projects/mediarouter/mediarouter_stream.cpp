//==============================================================================
//
//  MediaRouteStream
//
//  Created by Keukhan
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

// Role Definition
// ------------------------
// Inbound stream process
//		Calculating packet duration ->
//		Changing the bitstream format  ->
// 		Parsing Track Information ->
// 		Parsing Fragmentation Header ->
// 		Parsing Key-frame ->
// 		Append Decoder ParameterSets ->
// 		Changing the timestamp based on the timebase

// Outbound Stream process
//		Calculating packet duration ->
// 		Parsing Track Information ->
// 		Parsing Fragmentation Header ->
// 		Parsing Key-frame ->
// 		Append Decoder ParameterSets ->
// 		Changing the timestamp based on the timebase

#include "mediarouter_stream.h"

#include <base/ovlibrary/ovlibrary.h>

#include "mediarouter_private.h"

#define PTS_CORRECT_THRESHOLD_MS 3000

using namespace cmn;

MediaRouteStream::MediaRouteStream(const std::shared_ptr<info::Stream> &stream, MediaRouterStreamType type)
	: _stream(stream),
	  _packets_queue(nullptr, 600)
{
	SetType(type);

	MediaRouterStats::Init(stream);
}

MediaRouteStream::~MediaRouteStream()
{
	_media_packet_stash.clear();
	_packets_queue.Clear();

	_pts_last.clear();
}

std::shared_ptr<info::Stream> MediaRouteStream::GetStream()
{
	return _stream;
}

void MediaRouteStream::SetType(MediaRouterStreamType type)
{
	_type = type;

	auto urn = std::make_shared<info::ManagedQueue::URN>(
		_stream->GetApplicationInfo().GetVHostAppName(),
		_stream->GetName(),
		(_type == MediaRouterStreamType::INBOUND) ? "imr" : "omr",
		"streamworker");
	_packets_queue.SetUrn(urn);
}

void MediaRouteStream::OnStreamPrepared(bool completed)
{
	_is_stream_prepared = completed;
}

bool MediaRouteStream::IsStreamPrepared()
{
	return _is_stream_prepared;
}

void MediaRouteStream::Flush()
{
	// Clear queued packets
	_packets_queue.Clear();
	// Clear stashed Packets
	_media_packet_stash.clear();

	_is_all_tracks_parsed = false;

	_is_stream_prepared = false;
}

// Check whether the information extraction for all tracks has been completed.
bool MediaRouteStream::IsStreamReady()
{
	if (_is_all_tracks_parsed == true)
	{
		return true;
	}

	auto tracks = _stream->GetTracks();

	for (const auto &track_it : tracks)
	{
		auto track = track_it.second;
		if (track->IsValid() == false)
		{
			return false;
		}

		// If the track is an outbound track, it is necessary to check the quality.
		if (track->HasQualityMeasured() == false)
		{
			return false;
		}
	}

	_is_all_tracks_parsed = true;

	return true;
}

// @deprecated
void MediaRouteStream::DropNonDecodingPackets()
{
	////////////////////////////////////////////////////////////////////////////////////
	// 1. Discover to the highest PTS value in the keyframe against packets on all tracks.

	std::vector<std::shared_ptr<MediaPacket>> tmp_packets_queue;
	int64_t base_pts = -1LL;

	while (true)
	{
		if (_packets_queue.IsEmpty())
			break;

		auto media_packet_ref = _packets_queue.Dequeue();
		if (media_packet_ref.has_value() == false)
			continue;

		auto &media_packet = media_packet_ref.value();

		if (media_packet->GetFlag() == MediaPacketFlag::Key)
		{
			if (base_pts < media_packet->GetPts())
			{
				base_pts = media_packet->GetPts();
				logtw("Discovered base PTS value track_id:%d, flags:%d, size:%d,  pts:%lld", (int32_t)media_packet->GetTrackId(), media_packet->GetFlag(), media_packet->GetDataLength(), base_pts);
			}
		}

		tmp_packets_queue.push_back(std::move(media_packet));
	}

	////////////////////////////////////////////////////////////////////////////////////
	// 2. Obtain the PTS values for all tracks close to the reference PTS.

	// <TrackId, <diff, Pts>>
	std::map<MediaTrackId, std::pair<int64_t, int64_t>> map_near_pts;

	for (auto it = tmp_packets_queue.begin(); it != tmp_packets_queue.end(); it++)
	{
		auto &media_packet = *it;

		if (media_packet->GetFlag() == MediaPacketFlag::Key)
		{
			MediaTrackId track_id = media_packet->GetTrackId();

			int64_t pts_diff = std::abs(media_packet->GetPts() - base_pts);

			auto it_near_pts = map_near_pts.find(track_id);

			if (it_near_pts == map_near_pts.end())
			{
				map_near_pts[track_id] = std::make_pair(pts_diff, media_packet->GetPts());
			}
			else
			{
				auto pair_value = it_near_pts->second;
				int64_t prev_pts_diff = pair_value.first;

				if (prev_pts_diff > pts_diff)
				{
					map_near_pts[track_id] = std::make_pair(pts_diff, media_packet->GetPts());
				}
			}
		}
	}

	////////////////////////////////////////////////////////////////////////////////////
	// 3. Drop all packets below PTS by all tracks

	uint32_t dropped_packets = 0;
	for (auto it = tmp_packets_queue.begin(); it != tmp_packets_queue.end(); it++)
	{
		auto &media_packet = *it;

		if (media_packet->GetPts() < map_near_pts[media_packet->GetTrackId()].second)
		{
			dropped_packets++;
			continue;
		}

		_packets_queue.Enqueue(std::move(media_packet));
	}
	tmp_packets_queue.clear();

	if (dropped_packets > 0)
	{
		logtw("Number of dropped packets : %d", dropped_packets);
	}
}

void MediaRouteStream::Push(const std::shared_ptr<MediaPacket> &media_packet)
{
	_packets_queue.Enqueue(media_packet, media_packet->IsHighPriority());
}

std::shared_ptr<MediaPacket> MediaRouteStream::PopAndNormalize()
{
	// Get Media Packet
	if (_packets_queue.IsEmpty())
	{
		return nullptr;
	}

	auto media_packet_ref = _packets_queue.Dequeue();
	if (media_packet_ref.has_value() == false)
	{
		return nullptr;
	}

	auto &media_packet = media_packet_ref.value();

	////////////////////////////////////////////////////////////////////////////////////
	// [ Calculating Packet Timestamp, Duration]

	// If there is no pts/dts value, do the same as pts/dts value.
	if (media_packet->GetDts() == -1LL)
	{
		media_packet->SetDts(media_packet->GetPts());
	}

	if (media_packet->GetPts() == -1LL)
	{
		media_packet->SetPts(media_packet->GetDts());
	}

	// Accumulate Packet duplication
	//	- 1) If the current packet does not have a Duration value then stashed.
	//	- 1) If packets stashed, calculate duration compared to the current packet timestamp.
	//	- 3) and then, the current packet stash.
	std::shared_ptr<MediaPacket> pop_media_packet = nullptr;

	if (IsOutbound() &&
		(media_packet->GetDuration()) <= 0 &&
		// The packet duration recalculation applies only to video and audio types.
		(media_packet->GetMediaType() == MediaType::Video || media_packet->GetMediaType() == MediaType::Audio))
	{
		auto it = _media_packet_stash.find(media_packet->GetTrackId());
		if (it == _media_packet_stash.end())
		{
			_media_packet_stash[media_packet->GetTrackId()] = std::move(media_packet);

			return nullptr;
		}

		pop_media_packet = std::move(it->second);

		// [#743] Recording and HLS packetizing are failing due to non-monotonically increasing dts.
		// So, the code below is a temporary measure to avoid this problem. A more fundamental solution should be considered.
		if (pop_media_packet->GetDts() >= media_packet->GetDts())
		{
			if (_warning_count_out_of_order++ < 10)
			{
				logtw("[%s/%s] Detected out of order DTS of packet. track_id:%d dts:%lld->%lld",
					  _stream->GetApplicationName(), _stream->GetName().CStr(), pop_media_packet->GetTrackId(), pop_media_packet->GetDts(), media_packet->GetDts());
			}

			// If a packet has entered this function, it's a really weird stream.
			// It must be seen that the order of the packets is jumbled.
			media_packet->SetPts(pop_media_packet->GetPts() + 1);
			media_packet->SetDts(pop_media_packet->GetDts() + 1);
		}  // end of temporary measure code

		int64_t duration = media_packet->GetDts() - pop_media_packet->GetDts();
		pop_media_packet->SetDuration(duration);

		_media_packet_stash[media_packet->GetTrackId()] = std::move(media_packet);
	}
	else
	{
		pop_media_packet = std::move(media_packet);

		// The packet duration of data type is always 0.
		if (pop_media_packet->GetMediaType() == MediaType::Data)
		{
			pop_media_packet->SetDuration(0);
		}
	}

	////////////////////////////////////////////////////////////////////////////////////
	// Bitstream format converting to standard format. and, parsing track information
	auto media_type = pop_media_packet->GetMediaType();
	auto track_id = pop_media_packet->GetTrackId();

	auto media_track = _stream->GetTrack(track_id);
	if (media_track == nullptr)
	{
		logte("Could not find the media track. track_id: %d, media_type: %s",
			  track_id,
			  GetMediaTypeString(media_type).CStr());

		return nullptr;
	}

	// Convert bitstream format and normalize (e.g. Add SPS/PPS to head of H264 IDR frame)
	// @MediaRouterNormalize
	if (MediaRouterNormalize::NormalizeMediaPacket(GetStream(), media_track, pop_media_packet) == false)
	{
		return nullptr;
	}

	// Detect abnormal packet
	if (IsOutbound())
	{
		if (pop_media_packet->GetDuration() < 0)
		{
			logtw("[%s/%s] found invalid duration of packet. We need to find the cause of the incorrect Duration.", _stream->GetApplicationName(), _stream->GetName().CStr());
			return nullptr;
		}
	}

	// Detect abnormal packet
	if (IsInbound())
	{
		DetectAbnormalPackets(media_track, pop_media_packet);
	}


	media_track->OnFrameAdded(pop_media_packet);

	// Update statistics
	MediaRouterStats::Update(static_cast<uint8_t>(_type), IsStreamPrepared(), _packets_queue, GetStream(), media_track, pop_media_packet);

	// Mirror Buffer
	_mirror_buffer.emplace_back(std::make_shared<MirrorBufferItem>(pop_media_packet));

	// Delete old packets
	for (auto it = _mirror_buffer.begin(); it != _mirror_buffer.end();)
	{
		auto item = *it;
		if (item->GetElapsedMilliseconds() > 3000)
		{
			it = _mirror_buffer.erase(it);
		}
		else
		{
			++it;
		}
	}

	return pop_media_packet;
}

std::vector<std::shared_ptr<MediaRouteStream::MirrorBufferItem>> MediaRouteStream::GetMirrorBuffer()
{
	return _mirror_buffer;
}

void MediaRouteStream::DetectAbnormalPackets(std::shared_ptr<MediaTrack> &media_track, std::shared_ptr<MediaPacket> &packet)
{
	auto track_id = packet->GetTrackId();
	auto it = _pts_last.find(track_id);
	if (it != _pts_last.end())
	{
		int64_t ts_ms = packet->GetPts() * media_track->GetTimeBase().GetExpr();
		int64_t ts_diff_ms = ts_ms - _pts_last[track_id];

		if (std::abs(ts_diff_ms) > PTS_CORRECT_THRESHOLD_MS)
		{
			if (IsVideoCodec(media_track->GetCodecId()) || IsAudioCodec(media_track->GetCodecId()))
			{
				logtw("[%s/%s(%u)] Detected abnormal increased timestamp. track:%u last.pts: %lld, cur.pts: %lld, tb(%d/%d), diff: %lldms",
					  _stream->GetApplicationInfo().GetVHostAppName().CStr(),
					  _stream->GetName().CStr(),
					  _stream->GetId(),
					  track_id,
					  _pts_last[track_id],
					  packet->GetPts(),
					  media_track->GetTimeBase().GetNum(),
					  media_track->GetTimeBase().GetDen(),
					  ts_diff_ms);
			}
		}

		_pts_last[track_id] = ts_ms;
	}
}
