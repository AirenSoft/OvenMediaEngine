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

HlsPacketizer::HlsPacketizer(const ov::String &app_name, const ov::String &stream_name,
							 uint32_t segment_count, uint32_t segment_duration,
							 const std::shared_ptr<MediaTrack> &video_track, const std::shared_ptr<MediaTrack> &audio_track,
							 const std::shared_ptr<ChunkedTransferInterface> &chunked_transfer)
	: Packetizer(app_name, stream_name,
				 segment_count, segment_count * 5, segment_duration,
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
		logad("This stream has no video track");
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
		logad("This stream has no audio track");
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

bool HlsPacketizer::AppendVideoFrame(const std::shared_ptr<const MediaPacket> &media_packet)
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

	if (_ts_writer.PrepareIfNeeded() == false)
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
			result = WriteSegment(first_pts, first_pts * _video_timebase_expr_ms, duration, duration * _video_timebase_expr_ms);
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

bool HlsPacketizer::AppendAudioFrame(const std::shared_ptr<const MediaPacket> &media_packet)
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

	if (_ts_writer.PrepareIfNeeded() == false)
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
				result = WriteSegment(first_pts, first_pts * _audio_timebase_expr_ms, duration, duration * _audio_timebase_expr_ms);
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

	SetSegmentData(ov::String::FormatString("%u.ts", _sequence_number),
				   timestamp,
				   timestamp_in_ms,
				   duration,
				   duration_in_ms,
				   data);

	UpdatePlayList();

	if (_ts_writer.Prepare() == false)
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
	double max_duration_in_ms = 0;

	std::vector<std::shared_ptr<SegmentItem>> segment_datas;
	Packetizer::GetVideoPlaySegments(segment_datas);

	for (const auto &segment_data : segment_datas)
	{
		m3u8_play_list << "#EXTINF:" << std::fixed << std::setprecision(0)
					   << (segment_data->duration_in_ms / 1000) << "\r\n"
					   << segment_data->file_name.CStr() << "\r\n";

		if (segment_data->duration_in_ms > max_duration_in_ms)
		{
			max_duration_in_ms = segment_data->duration_in_ms;
		}
	}

	play_list_stream << "#EXTM3U\r\n"
					 << "#EXT-X-VERSION:3\r\n"
					 << "#EXT-X-MEDIA-SEQUENCE:" << (_sequence_number - 1) << "\r\n"
					 << "#EXT-X-ALLOW-CACHE:NO\r\n"
					 << "#EXT-X-TARGETDURATION:" << std::fixed << std::setprecision(0) << (max_duration_in_ms / 1000) << "\r\n"
					 << m3u8_play_list.str();

	ov::String play_list = play_list_stream.str().c_str();

	// logad("%p %d %s", this, IsReadyForStreaming(), play_list.CStr());

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

	// video segment mutex
	std::unique_lock<std::mutex> lock(_video_segment_mutex);

	auto item = std::find_if(_video_segments.begin(),
							 _video_segments.end(), [&](std::shared_ptr<SegmentItem> const &value) -> bool {
								 return value != nullptr ? value->file_name == file_name : false;
							 });

	if (item == _video_segments.end())
	{
		return nullptr;
	}

	return (*item);
}

bool HlsPacketizer::SetSegmentData(ov::String file_name, int64_t timestamp, int64_t timestamp_in_ms, int64_t duration, int64_t duration_in_ms, const std::shared_ptr<const ov::Data> &data)
{
	auto segment_data = std::make_shared<SegmentItem>(
		SegmentDataType::Both,
		_sequence_number++,
		file_name,
		timestamp,
		timestamp_in_ms,
		duration,
		duration_in_ms,
		data);

	// video segment mutex
	std::unique_lock<std::mutex> lock(_video_segment_mutex);

	_video_segments[_current_video_index++] = segment_data;

	if (_segment_save_count <= _current_video_index)
	{
		_current_video_index = 0;
	}

	logad("TS segment is added, file: %s, pts: %" PRId64 "ms, duration: %" PRIu64 "ms, data size: %zubytes", file_name.CStr(), timestamp_in_ms, duration_in_ms, data->GetLength());

	if ((IsReadyForStreaming() == false) && (_sequence_number > _segment_count))
	{
		SetReadyForStreaming();

		logai("Segments are ready, segment duration: %fs, count: %u",
			  _segment_duration, _segment_count);
	}

	DumpSegmentToFile(segment_data);

	return true;
}
