//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#include "hls_packetizer.h"

#include <base/ovlibrary/ovlibrary.h>
#include <modules/segment_writer/writer.h>
#include <publishers/segment/segment_stream/packetizer/packetizer_define.h>

#include <algorithm>
#include <array>
#include <iomanip>
#include <sstream>

#include "hls_private.h"

static inline void DumpSegmentToFile(const std::shared_ptr<const SegmentItem> &segment_item)
{
#if DEBUG
	static bool dump = ov::Converter::ToBool(std::getenv("OME_DUMP_HLS"));

	if (dump)
	{
		auto &file_name = segment_item->file_name;
		auto &data = segment_item->data;

		ov::DumpToFile(ov::PathManager::Combine(ov::PathManager::GetAppPath("dump/hls"), file_name), data);
	}
#endif	// DEBUG
}

HlsPacketizer::HlsPacketizer(const ov::String &service_name, const ov::String &app_name, const ov::String &stream_name,
							 uint32_t segment_count, uint32_t segment_duration,
							 const std::shared_ptr<MediaTrack> &video_track, const std::shared_ptr<MediaTrack> &audio_track,
							 const std::shared_ptr<ChunkedTransferInterface> &chunked_transfer)
	: Packetizer(service_name, app_name, stream_name,
				 segment_count, segment_count * 2, segment_duration,
				 video_track, audio_track,
				 chunked_transfer),

	  _ts_writer(Writer::Type::MpegTs, Writer::MediaType::Both)
{
	_video_enable = false;
	_audio_enable = false;

	SetVideoTrack(video_track);
	SetAudioTrack(audio_track);

	_stat_stop_watch.Start();
}

void HlsPacketizer::SetVideoTrack(const std::shared_ptr<MediaTrack> &video_track)
{
	if (video_track == nullptr)
	{
		logaw("This stream has no video track");
		return;
	}

	switch (video_track->GetCodecId())
	{
		case cmn::MediaCodecId::H264:
		case cmn::MediaCodecId::H265: {
			auto &timebase = video_track->GetTimeBase();
			_video_timebase_expr = timebase.GetExpr();
			_video_timebase_expr_ms = (_video_timebase_expr * 1000.0);
			_video_timescale = timebase.GetTimescale();

			if ((_video_timebase_expr == 0.0) || (_video_timescale == 0.0))
			{
				logae("Video track is disabled because the timebase of the video track is not valid: %s",
					  video_track->GetTimeBase().ToString().CStr());
			}
			else
			{
				if (_ts_writer.AddTrack(video_track))
				{
					_ideal_duration_for_video = _segment_duration * _video_timescale;
					_ideal_duration_for_video_in_ms = static_cast<int64_t>(_segment_duration * 1000);

					_video_enable = true;
				}
			}

			break;
		}

		default:
			logaw("Not supported video codec: %s", ::StringFromMediaCodecId(video_track->GetCodecId()).CStr());
			break;
	}
}

void HlsPacketizer::SetAudioTrack(const std::shared_ptr<MediaTrack> &audio_track)
{
	if (audio_track == nullptr)
	{
		logaw("This stream has no audio track");
		return;
	}

	switch (audio_track->GetCodecId())
	{
		case cmn::MediaCodecId::Aac:
		case cmn::MediaCodecId::Mp3: {
			auto &timebase = audio_track->GetTimeBase();
			_audio_timebase_expr = timebase.GetExpr();
			_audio_timebase_expr_ms = (_audio_timebase_expr * 1000.0);
			_audio_timescale = timebase.GetTimescale();

			if ((_audio_timebase_expr == 0.0) || (_audio_timescale == 0.0))
			{
				logae("Audio track is disabled because the timebase of the video track is not valid: %s",
					  audio_track->GetTimeBase().ToString().CStr());
			}
			else
			{
				if (_ts_writer.AddTrack(audio_track))
				{
					_ideal_duration_for_audio = _segment_duration * _audio_timescale;
					_ideal_duration_for_audio_in_ms = static_cast<int64_t>(_segment_duration * 1000);

					_audio_enable = true;
				}
			}
			break;
		}

		default:

			// Not supported codec
			break;
	}
}

HlsPacketizer::~HlsPacketizer()
{
	_ts_writer.Finalize();
}

bool HlsPacketizer::ResetPacketizer(uint32_t new_msid)
{
	std::lock_guard lock_guard(_flush_mutex);
	_last_msid = new_msid;
	_need_to_flush = true;

	logai("Packetizer will be reset: %u", new_msid);

	return true;
}

uint32_t HlsPacketizer::FlushIfNeeded()
{
	// Even if the _last_msid value changes from the outside while the packet is being processed,
	// it is stored to temporary variable to minimize the side effect.
	bool need_to_flush = false;
	uint32_t last_msid = _last_msid;

	if (_need_to_flush)
	{
		std::lock_guard lock_guard(_flush_mutex);

		// DCL
		if (_need_to_flush)
		{
			need_to_flush = true;
			_need_to_flush = false;

			last_msid = _last_msid;
		}
	}

	if (need_to_flush)
	{
		if (_ts_writer.GetData() != nullptr)
		{
			auto target_track = (_video_track != nullptr) ? _video_track : _audio_track;

			if (target_track != nullptr)
			{
				auto track_id = target_track->GetId();
				auto timebase_expr_ms = (_video_track != nullptr) ? _video_timebase_expr_ms : _audio_timebase_expr_ms;

				// Flush TS writer
				auto first_pts = _ts_writer.GetFirstPts(track_id);
				auto duration = _ts_writer.GetDuration(track_id);

				WriteSegment(
					first_pts, first_pts * timebase_expr_ms,
					duration, duration * timebase_expr_ms);

				_video_segment_queue.MarkAsDiscontinuity();
			}

			_first_video_pts = -1LL;
			_first_audio_pts = -1LL;
			_last_video_pts = -1LL;
			_last_audio_pts = -1LL;
		}
	}

	return last_msid;
}

bool HlsPacketizer::AppendVideoPacket(const std::shared_ptr<const MediaPacket> &media_packet)
{
	auto video_track = _video_track;

	if (video_track == nullptr)
	{
		// Video frame is sent since video track is not available
		OV_ASSERT2(false);

		return false;
	}

	if (_video_enable == false)
	{
		// Video is disabled (invalid timebase or not supported codec, etc...)
		return false;
	}

	auto current_msid = FlushIfNeeded();

	if (media_packet->GetMsid() != current_msid)
	{
		// Ignore the packet
		logtd("The packet is ignored by msid: (current msid: %u, packet msid: %u)", current_msid, media_packet->GetMsid());
		return true;
	}

	if (_video_key_frame_received == false)
	{
		// Wait for first key frame
		if (media_packet->GetFlag() != MediaPacketFlag::Key)
		{
			// Skip the frame
			return true;
		}

		_video_key_frame_received = true;
	}

	if (_ts_writer.PrepareIfNeeded(GetServiceName()) == false)
	{
		logte("Could not prepare ts writer");
		return false;
	}

	bool result = true;

	if ((media_packet->GetFlag() == MediaPacketFlag::Key) && (_first_video_pts >= 0L))
	{
		auto first_pts = _ts_writer.GetFirstPts(media_packet->GetTrackId());
		auto duration = std::max(
			media_packet->GetPts() - first_pts,
			_ts_writer.GetDuration(media_packet->GetTrackId()));

		if (duration >= _ideal_duration_for_video)
		{
			result = WriteSegment(
				first_pts, first_pts * _video_timebase_expr_ms,
				duration, duration * _video_timebase_expr_ms);
		}
	}

	result = result && _ts_writer.WritePacket(media_packet);

	if (result)
	{
		_first_video_pts = (_first_video_pts == -1L) ? media_packet->GetPts() : _first_video_pts;
		_last_video_pts = media_packet->GetPts();
	}

	return result;
}

bool HlsPacketizer::AppendAudioPacket(const std::shared_ptr<const MediaPacket> &media_packet)
{
	auto audio_track = _audio_track;
	if (audio_track == nullptr)
	{
		// Audio frame is sent since audio track is not available
		OV_ASSERT2(false);

		return false;
	}

	if (_audio_enable == false)
	{
		// Audio is disabled (invalid timebase or not supported codec, etc...)
		return false;
	}

	auto current_msid = FlushIfNeeded();

	if (media_packet->GetMsid() != current_msid)
	{
		// Ignore the packet
		logtd("The packet is ignored by msid: (current msid: %u, packet msid: %u)", current_msid, media_packet->GetMsid());
		return true;
	}

	if (_audio_key_frame_received == false)
	{
		// Currently, the audio packet is always a Keyframe, but it will treat it the same as the video later for other codecs

		// Wait for first key frame
		if (media_packet->GetFlag() != MediaPacketFlag::Key)
		{
			// Skip the frame
			return true;
		}

		_audio_key_frame_received = true;
	}

	if (_ts_writer.PrepareIfNeeded(GetServiceName()) == false)
	{
		logte("Could not prepare ts writer");
		return false;
	}

	bool result = true;

	if ((media_packet->GetFlag() == MediaPacketFlag::Key) && (_first_audio_pts >= 0L))
	{
		auto first_pts = _ts_writer.GetFirstPts(media_packet->GetTrackId());
		auto duration = std::max(
			media_packet->GetPts() - first_pts,
			_ts_writer.GetDuration(media_packet->GetTrackId()));

		if (duration >= _ideal_duration_for_audio)
		{
			// Writing a segment is done only on the video side
			if (_video_enable == false)
			{
				result = WriteSegment(
					first_pts, first_pts * _audio_timebase_expr_ms,
					duration, duration * _audio_timebase_expr_ms);
			}
		}
	}

	result = result && _ts_writer.WritePacket(media_packet);

	if (result)
	{
		_first_audio_pts = (_first_audio_pts == -1L) ? media_packet->GetPts() : _first_audio_pts;
		_last_audio_pts = media_packet->GetPts();
	}

	return result;
}

bool HlsPacketizer::WriteSegment(int64_t timestamp, int64_t timestamp_in_ms, int64_t duration, int64_t duration_in_ms)
{
	auto data = _ts_writer.Finalize();

	if (data == nullptr)
	{
		// Data must be not nullptr
		logac("TS data is null");
		OV_ASSERT2(false);
		return false;
	}

	SetSegmentData(
		timestamp, timestamp_in_ms,
		duration, duration_in_ms,
		data);

	if (_ts_writer.Prepare(GetServiceName()) == false)
	{
		logae("Could not prepare ts writer");
		return false;
	}

	_video_ready = false;
	_audio_ready = false;

	return true;
}

bool HlsPacketizer::UpdatePlayList()
{
	std::ostringstream play_list_stream;
	std::ostringstream m3u8_play_list;
	double max_duration_in_ms = 0.0;

	bool is_first = true;
	auto first_sequence_number = 0U;
	auto current_discontinuity_count = 0U;

	_video_segment_queue.Iterate([&](std::shared_ptr<const SegmentItem> segment_item) -> bool {
		if (is_first)
		{
			is_first = false;
			first_sequence_number = segment_item->sequence_number;
		}

		m3u8_play_list << "#EXTINF:" << std::fixed << std::setprecision(0)
					   << (segment_item->duration_in_ms / 1000) << "\r\n"
					   << segment_item->file_name.CStr() << "\r\n";

		if (segment_item->duration_in_ms > max_duration_in_ms)
		{
			max_duration_in_ms = segment_item->duration_in_ms;
		}

		if (segment_item->discontinuity)
		{
			current_discontinuity_count++;
			m3u8_play_list << "#EXT-X-DISCONTINUITY\r\n";
		}

		return true;
	});

	if (_last_discontinuity_count > current_discontinuity_count)
	{
		_discontinuity_sequence_number += (_last_discontinuity_count - current_discontinuity_count);
	}

	_last_discontinuity_count = current_discontinuity_count;

	play_list_stream
		<< "#EXTM3U\r\n"
		<< "#EXT-X-VERSION:3\r\n"
		<< "#EXT-X-MEDIA-SEQUENCE:" << first_sequence_number << "\r\n";

	if (_discontinuity_sequence_number > 0)
	{
		play_list_stream
			<< "#EXT-X-DISCONTINUITY-SEQUENCE:" << _discontinuity_sequence_number << "\r\n";
	}

	play_list_stream
		<< "#EXT-X-ALLOW-CACHE:NO\r\n"
		<< "#EXT-X-TARGETDURATION:" << std::fixed << std::setprecision(0) << (max_duration_in_ms / 1000) << "\r\n"
		<< m3u8_play_list.str();

	ov::String play_list = play_list_stream.str().c_str();

	SetPlayList(play_list);

	if (_stat_stop_watch.IsElapsed(5000) && _stat_stop_watch.Update())
	{
		if ((_video_track != nullptr) && (_audio_track != nullptr))
		{
			auto audio_pts_in_ms = static_cast<int64_t>(_ts_writer.GetFirstPts(cmn::MediaType::Audio) * _audio_timebase_expr_ms);
			auto video_pts_in_ms = static_cast<int64_t>(_ts_writer.GetFirstPts(cmn::MediaType::Video) * _video_timebase_expr_ms);
			auto delta_in_ms = audio_pts_in_ms - video_pts_in_ms;

			logas("A-V Sync: %lldms (A: %lldms, V: %lldms)", delta_in_ms, audio_pts_in_ms, video_pts_in_ms);
		}
	}

	return true;
}

std::shared_ptr<const SegmentItem> HlsPacketizer::GetSegmentData(const ov::String &file_name) const
{
	if (IsReadyForStreaming() == false)
	{
		return nullptr;
	}

	return _video_segment_queue.GetSegmentData(file_name);
}

bool HlsPacketizer::SetSegmentData(int64_t timestamp, int64_t timestamp_in_ms, int64_t duration, int64_t duration_in_ms, const std::shared_ptr<const ov::Data> &data)
{
	auto file_name = ov::String::FormatString("%u.ts", _sequence_number);

	auto segment_item = _video_segment_queue.Append(
		SegmentDataType::Both, _sequence_number++,
		file_name,
		timestamp, timestamp_in_ms,
		duration, duration_in_ms,
		data);

	DumpSegmentToFile(segment_item);

	if (
		(IsReadyForStreaming() == false) &&
		(_video_segment_queue.GetCount() >= _segment_count))
	{
		SetReadyForStreaming();

		logai("Segments are ready, segment duration: %fs, count: %u",
			  _segment_duration, _segment_count);
	}

	return UpdatePlayList();
}
