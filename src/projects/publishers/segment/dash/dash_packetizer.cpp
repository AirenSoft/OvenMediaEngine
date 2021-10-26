﻿//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#include "dash_packetizer.h"

#include <modules/bitstream/aac/aac_converter.h>

#include <algorithm>
#include <iomanip>
#include <numeric>
#include <sstream>

#include "dash_define.h"
#include "dash_private.h"

static inline void DumpSegmentToFile(const std::shared_ptr<const SegmentItem> &segment_item)
{
#if DEBUG
	static bool dump = ov::Converter::ToBool(std::getenv("OME_DUMP_DASH"));

	if (dump)
	{
		auto &file_name = segment_item->file_name;
		auto &data = segment_item->data;

		ov::DumpToFile(ov::PathManager::Combine(ov::PathManager::GetAppPath("dump/dash"), file_name), data);
	}
#endif	// DEBUG
}

DashPacketizer::DashPacketizer(const ov::String &service_name, const ov::String &app_name, const ov::String &stream_name,
							   uint32_t segment_count, uint32_t segment_duration,
							   const ov::String &utc_timing_scheme, const ov::String &utc_timing_value,
							   std::shared_ptr<MediaTrack> video_track, std::shared_ptr<MediaTrack> audio_track,
							   const std::shared_ptr<ChunkedTransferInterface> &chunked_transfer)
	: Packetizer(service_name, app_name, stream_name,
				 segment_count, segment_count * 2, segment_duration,
				 video_track, audio_track,
				 chunked_transfer),

	  _utc_timing_scheme(utc_timing_scheme),
	  _utc_timing_value(utc_timing_value),

	  _video_m4s_writer(Writer::Type::M4s, Writer::MediaType::Video),
	  _audio_m4s_writer(Writer::Type::M4s, Writer::MediaType::Audio)
{
	_mpd_min_buffer_time = 6;

	SetVideoTrack(video_track);
	SetAudioTrack(audio_track);

	_stat_stop_watch.Start();

	_utc_timing_value = _utc_timing_value.Replace("&", "&amp;");
}

DashPacketizer::~DashPacketizer()
{
	_video_m4s_writer.Finalize();
	_audio_m4s_writer.Finalize();
}

void DashPacketizer::SetVideoTrack(const std::shared_ptr<MediaTrack> &video_track)
{
	if (video_track == nullptr)
	{
		// This stream has no video track
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
				if (_video_m4s_writer.AddTrack(video_track))
				{
					_ideal_duration_for_video = _segment_duration * _video_timescale;
					_ideal_duration_for_video_in_ms = static_cast<int64_t>(_segment_duration * 1000);

					_video_enable = true;
				}
				else
				{
					logae("Could not prepare video writer. Video track will be disabled");
				}
			}

			break;
		}

		default:
			logaw("Not supported video codec: %s", ::StringFromMediaCodecId(video_track->GetCodecId()).CStr());
			break;
	}
}

void DashPacketizer::SetAudioTrack(const std::shared_ptr<MediaTrack> &audio_track)
{
	if (audio_track == nullptr)
	{
		// This stream has no audio track
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
				logae("Audio track is disabled because the timebase of the audio track is not valid: %s",
					  audio_track->GetTimeBase().ToString().CStr());
			}
			else
			{
				if (_audio_m4s_writer.AddTrack(audio_track))
				{
					_ideal_duration_for_audio = _segment_duration * _audio_timescale;
					_ideal_duration_for_audio_in_ms = static_cast<int64_t>(_segment_duration * 1000);

					_audio_enable = true;
				}
				else
				{
					logae("Could not prepare audio writer. Audio track will be disabled");
				}
			}

			break;
		}

		default:
			logaw("Not supported audio codec: %s", ::StringFromMediaCodecId(audio_track->GetCodecId()).CStr());
			break;
	}
}

DashFileType DashPacketizer::GetFileType(const ov::String &file_name)
{
	if (file_name == DASH_MPD_VIDEO_INIT_FILE_NAME)
	{
		return DashFileType::VideoInit;
	}
	else if (file_name == DASH_MPD_AUDIO_INIT_FILE_NAME)
	{
		return DashFileType::AudioInit;
	}
	else if (file_name.HasSuffix(DASH_MPD_VIDEO_FULL_SUFFIX))
	{
		return DashFileType::VideoSegment;
	}
	else if (file_name.HasSuffix(DASH_MPD_AUDIO_FULL_SUFFIX))
	{
		return DashFileType::AudioSegment;
	}

	return DashFileType::Unknown;
}

ov::String DashPacketizer::GetFileName(int segment_index, cmn::MediaType media_type) const
{
	switch (media_type)
	{
		case cmn::MediaType::Video:
			// 1234_video.m4s
			return ov::String::FormatString("%d%s", segment_index, DASH_MPD_VIDEO_FULL_SUFFIX);

		case cmn::MediaType::Audio:
			// 1234_audio.m4s
			return ov::String::FormatString("%d%s", segment_index, DASH_MPD_AUDIO_FULL_SUFFIX);

		default:
			break;
	}

	return "";
}

bool DashPacketizer::PrepareVideoInitIfNeeded()
{
	if (_video_init_file != nullptr)
	{
		return true;
	}

	if ((_video_m4s_writer.Prepare(GetServiceName()) && _video_m4s_writer.Flush()) == false)
	{
		return false;
	}

	auto init_data = _video_m4s_writer.GetData();

	if (init_data == nullptr)
	{
		return false;
	}

	if (_video_m4s_writer.ResetData() == false)
	{
		return false;
	}

	auto video_track = _video_track;
	_video_init_file = std::make_shared<SegmentItem>(SegmentDataType::Video, 0, DASH_MPD_VIDEO_INIT_FILE_NAME, 0, 0, 0, 0, init_data);

	uint32_t resolution_gcd = std::gcd(video_track->GetWidth(), video_track->GetHeight());

	if (resolution_gcd != 0)
	{
		_pixel_aspect_ratio.Format("%d:%d", video_track->GetWidth() / resolution_gcd, video_track->GetHeight() / resolution_gcd);
	}

	DumpSegmentToFile(_video_init_file);

	logai("%s has created", DASH_MPD_VIDEO_INIT_FILE_NAME);

	return true;
}

bool DashPacketizer::PrepareAudioInitIfNeeded()
{
	if (_audio_init_file != nullptr)
	{
		return true;
	}

	if ((_audio_m4s_writer.Prepare(GetServiceName()) && _audio_m4s_writer.Flush()) == false)
	{
		return false;
	}

	auto init_data = _audio_m4s_writer.GetData();

	if (init_data == nullptr)
	{
		return false;
	}

	if (_audio_m4s_writer.ResetData() == false)
	{
		return false;
	}

	auto audio_track = _audio_track;
	_audio_init_file = std::make_shared<SegmentItem>(SegmentDataType::Audio, 0, DASH_MPD_AUDIO_INIT_FILE_NAME, 0, 0, 0, 0, init_data);

	DumpSegmentToFile(_audio_init_file);

	logai("%s has created", DASH_MPD_AUDIO_INIT_FILE_NAME);

	return true;
}

bool DashPacketizer::WriteVideoSegment()
{
	if (_video_m4s_writer.Flush() == false)
	{
		logae("Could not flush video track");
		return false;
	}

	auto duration = _video_m4s_writer.GetDuration();
	auto pts = std::move(_first_video_pts);

	if (_video_start_time == -1L)
	{
		_video_start_time = ov::Time::GetTimestampInMs() - (duration * _video_timebase_expr_ms);
	}

	if (SetSegmentData(_video_m4s_writer, pts))
	{
		_duration_delta_for_video += (_ideal_duration_for_video - duration);
		_first_video_pts += duration;

		UpdatePlayList();

		return true;
	}
	else
	{
		logae("Failed to set segment data");
	}

	return false;
}

bool DashPacketizer::WriteAudioSegment()
{
	if (_audio_m4s_writer.Flush() == false)
	{
		logae("Could not flush audio track");
		return false;
	}

	auto duration = _audio_m4s_writer.GetDuration();
	auto pts = std::move(_first_audio_pts);

	if (_audio_start_time == -1L)
	{
		_audio_start_time = ov::Time::GetTimestampInMs() - (duration * _audio_timebase_expr_ms);
	}

	if (SetSegmentData(_audio_m4s_writer, pts))
	{
		_duration_delta_for_audio += (_ideal_duration_for_audio - duration);
		_first_audio_pts = -1;

		UpdatePlayList();

		return true;
	}
	else
	{
		logae("Failed to set segment data");
	}

	return false;
}

bool DashPacketizer::ResetPacketizer(uint32_t new_msid)
{
	return true;
}

bool DashPacketizer::AppendVideoPacket(const std::shared_ptr<const MediaPacket> &media_packet)
{
	// logap("#%d [%s] Received a packet: %15ld, %15ld",
	// 	  media_packet->GetTrackId(), (media_packet->GetMediaType() == cmn::MediaType::Video) ? "V" : "A",
	// 	  media_packet->GetPts(), media_packet->GetDts());

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

	if (PrepareVideoInitIfNeeded() == false)
	{
		logae("Could not prepare video init");
		_video_enable = false;
		return false;
	}

	bool result = true;

	_first_video_pts = (_first_video_pts == -1L) ? media_packet->GetPts() : _first_video_pts;

	if (media_packet->GetFlag() == MediaPacketFlag::Key)
	{
		auto duration = std::max(
			media_packet->GetPts() - _video_m4s_writer.GetFirstPts(),
			_video_m4s_writer.GetDuration());

		if ((duration - _duration_delta_for_video) >= _ideal_duration_for_video)
		{
			result = WriteVideoSegment();
		}
	}

	result = result && _video_m4s_writer.WritePacket(media_packet);

	if (result)
	{
		_last_video_pts = media_packet->GetPts();
	}

	return result;
}

bool DashPacketizer::AppendAudioPacket(const std::shared_ptr<const MediaPacket> &media_packet)
{
	// logap("#%d [%s] Received a packet: %15ld, %15ld",
	// 	  media_packet->GetTrackId(), (media_packet->GetMediaType() == cmn::MediaType::Video) ? "V" : "A",
	// 	  media_packet->GetPts(), media_packet->GetDts());

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

	_audio_key_frame_received = true;

	if (PrepareAudioInitIfNeeded() == false)
	{
		logae("Could not prepare audio init");
		_audio_enable = false;
		return false;
	}

	auto data = media_packet->GetData();
	std::vector<size_t> length_list;

	if (media_packet->GetBitstreamFormat() == cmn::BitstreamFormat::AAC_ADTS)
	{
		data = AacConverter::ConvertAdtsToRaw(data, &length_list);
	}
	else
	{
		length_list.push_back(data->GetLength());
	}

	if (length_list.size() == 0)
	{
		// Invalid data
		logae("Length list must contains at least 1 item");
		return false;
	}

	bool result = true;
	_first_audio_pts = (_first_audio_pts >= 0L) ? _first_audio_pts : media_packet->GetPts();

	// Multiple frames can come together in on media_packet, which must be split up
	// TODO(dimiden): If media_packet->GetLength() is twice as long as _ideal_duration_for_audio, some problems can occur that can be packaged disproportionately

	// Calculates the time remaining to reach _ideal_duration_for_audio
	auto remained = _ideal_duration_for_audio - (_audio_m4s_writer.GetDuration() - _duration_delta_for_audio);

	// logac("#%d [%s] Received a packet: %15ld, %15ld (%ld, %zu)",
	// 	  media_packet->GetTrackId(), (media_packet->GetMediaType() == cmn::MediaType::Video) ? "V" : "A",
	// 	  media_packet->GetPts(), media_packet->GetDts(),
	// 	  _first_audio_pts, length_list.size());

	if (media_packet->GetDuration() < remained)
	{
		result = result && _audio_m4s_writer.WritePacket(media_packet, data, length_list, 0, length_list.size());

		if (result)
		{
			_last_audio_pts = media_packet->GetPts();
		}
	}
	else
	{
		// Split and store data of media_packet to fit the remained time
		int64_t frame_count = length_list.size();
		int64_t duration_per_frame = media_packet->GetDuration() / frame_count;

		if ((frame_count == 0) || (duration_per_frame == 0))
		{
			logae("Invalid media frame count (%ld) or packet duration (%ld, %ld)", frame_count, duration_per_frame, frame_count);
			return false;
		}

		// Calculate a value that matches this formula: [duration_per_frame * position >= remained]
		int64_t position = std::max(remained / duration_per_frame, 0L);
		int64_t bytes_to_copy = 0L;

		for (int64_t index = 0; index < position; index++)
		{
			bytes_to_copy += length_list[index];
		}

		// Split length_list
		std::vector<size_t> remained_list;
		remained_list.insert(remained_list.begin(), length_list.begin() + position, length_list.end());
		length_list.resize(position);

		if (position > 0)
		{
			result = result && _audio_m4s_writer.WritePacket(media_packet, data, length_list, 0, frame_count);
		}

		auto next_first_pts = _first_audio_pts + _audio_m4s_writer.GetDuration();
		result = WriteAudioSegment();
		_first_audio_pts = next_first_pts;

		if (remained_list.empty() == false)
		{
			data = data->Subdata(bytes_to_copy);
			result = result && _audio_m4s_writer.WritePacket(media_packet, data, remained_list, position, frame_count);
		}

		// TODO(dimiden): Need to set the PTS of last frame
		_last_audio_pts = media_packet->GetPts();
	}

	return result;
}

bool DashPacketizer::UpdatePlayList()
{
	std::ostringstream xml;

	if (IsReadyForStreaming() == false)
	{
		return false;
	}

	ov::String publish_time = ov::Time::MakeUtcSecond();

	logtd("Trying to update playlist for DASH with availabilityStartTime: %s, publishTime: %s", _start_time.CStr(), publish_time.CStr());

	xml << std::fixed << std::setprecision(3)
		<< R"(<?xml version="1.0" encoding="utf-8"?>)" << std::endl

		// MPD
		<< R"(<MPD xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance")" << std::endl
		<< R"(	xmlns="urn:mpeg:dash:schema:mpd:2011")" << std::endl
		<< R"(	xsi:schemaLocation="urn:mpeg:DASH:schema:MPD:2011 http://standards.iso.org/ittf/PubliclyAvailableStandards/MPEG-DASH_schema_files/DASH-MPD.xsd")" << std::endl
		<< R"(	profiles="urn:mpeg:dash:profile:isoff-live:2011")" << std::endl
		<< R"(	type="dynamic")" << std::endl
		<< R"(	minimumUpdatePeriod="PT30S")" << std::endl
		<< R"(	publishTime=")" << publish_time.CStr() << R"(")" << std::endl
		<< R"(	availabilityStartTime=")" << _start_time.CStr() << R"(")" << std::endl
		<< R"(	timeShiftBufferDepth="PT)" << (_segment_save_count * _segment_duration) << R"(S")" << std::endl
		<< R"(	maxSegmentDuration="PT)" << _segment_duration << R"(S")" << std::endl
		<< R"(	minBufferTime="PT)" << _segment_duration << R"(S">)" << std::endl;

	{
		xml
			// <Period>
			<< R"(	<Period start="PT0.000S">)" << std::endl;

		if (_video_track != nullptr)
		{
			xml
				// <AdaptationSet>
				<< R"(		<AdaptationSet )"
				<< R"(mimeType="video/mp4" )"
				<< R"(contentType="video" )"
				<< R"(segmentAlignment="true" )"
				<< R"(startWithSAP="1" )"
				<< R"(par=")" << _pixel_aspect_ratio << R"(">)" << std::endl;

			{
				xml
					// <Role />
					<< R"(			<Role )"
					<< R"(schemeIdUri="urn:mpeg:dash:role:2011" )"
					<< R"(value="main" />)" << std::endl;
			}

			{
				xml
					// <SegmentTemplate />
					<< R"(			<SegmentTemplate )"
					<< R"(startNumber="0" )"
					<< R"(timescale=")" << static_cast<int64_t>(_video_timescale) << R"(" )"
					<< R"(duration=")" << static_cast<int64_t>(_segment_duration * _video_timescale) << R"(" )"
					<< R"(initialization=")" << DASH_MPD_VIDEO_INIT_FILE_NAME << R"(" )"
					<< R"(media="$Number$)" << DASH_MPD_VIDEO_FULL_SUFFIX << R"(" />)" << std::endl;
			}

			{
				xml
					// <Representation />
					<< R"(			<Representation )"
					<< R"(mimeType="video/mp4" )"
					<< R"(id="video)" << _video_track->GetId() << R"(" )"
					<< R"(codecs=")" << GetCodecString(_video_track).CStr() << R"(" )"
					<< R"(bandwidth=")" << _video_track->GetBitrate() << R"(" )"
					<< R"(width=")" << _video_track->GetWidth() << R"(" )"
					<< R"(height=")" << _video_track->GetHeight() << R"(" )"
					<< R"(frameRate=")" << _video_track->GetFrameRate() << R"(" />)" << std::endl;
			}

			xml
				// </AdaptationSet>
				<< R"(		</AdaptationSet>)" << std::endl;
		}

		if (_audio_track != nullptr)
		{
			xml
				// <AdaptationSet>
				<< R"(		<AdaptationSet )"
				<< R"(mimeType="audio/mp4" )"
				<< R"(lang="und" )"
				<< R"(contentType="audio" )"
				<< R"(subsegmentAlignment="true" )"
				<< R"(subsegmentStartsWithSAP="1" )"
				<< R"(par=")" << _pixel_aspect_ratio << R"(">)" << std::endl;

			{
				xml
					// <Role />
					<< R"(			<Role )"
					<< R"(schemeIdUri="urn:mpeg:dash:role:2011" )"
					<< R"(value="main" />)" << std::endl;
			}

			{
				xml
					// <SegmentTemplate />
					<< R"(			<SegmentTemplate )"
					<< R"(startNumber="0" )"
					<< R"(timescale=")" << static_cast<int64_t>(_audio_timescale) << R"(" )"
					<< R"(duration=")" << static_cast<int64_t>(_segment_duration * _audio_timescale) << R"(" )"
					<< R"(initialization=")" << DASH_MPD_AUDIO_INIT_FILE_NAME << R"(" )"
					<< R"(media="$Number$)" << DASH_MPD_AUDIO_FULL_SUFFIX << R"(" />)" << std::endl;
			}

			{
				xml
					// <Representation>
					<< R"(			<Representation )"
					<< R"(id="audio)" << _audio_track->GetId() << R"(" )"
					<< R"(codecs=")" << GetCodecString(_audio_track).CStr() << R"(" )"
					<< R"(bandwidth=")" << _audio_track->GetBitrate() << R"(" )"
					<< R"(audioSamplingRate=")" << _audio_track->GetSampleRate() << R"(">)" << std::endl;

				{
					xml
						// <AudioChannelConfiguration />
						<< R"(				<AudioChannelConfiguration )"
						<< R"(schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" )"
						<< R"(value=")" << _audio_track->GetChannel().GetCounts() << R"(" />)" << std::endl;
				}

				xml
					// </Representation>
					<< R"(			</Representation>)" << std::endl;
			}

			xml
				// </AdaptationSet>
				<< R"(		</AdaptationSet>)" << std::endl;
		}

		xml <<
			// </Period>
			R"(	</Period>)" << std::endl;
	}

	if ((_utc_timing_scheme.IsEmpty() == false) && (_utc_timing_value.IsEmpty() == false))
	{
		xml
			// <UTCTiming />
			<< R"(	<UTCTiming schemeIdUri=")" << _utc_timing_scheme.CStr() << R"(" value=")" << _utc_timing_value.CStr() << R"(" />)" << std::endl;
	}

	xml
		// </MPD>
		<< R"(</MPD>)";

	ov::String play_list = xml.str().c_str();

	SetPlayList(play_list);

	if (_stat_stop_watch.IsElapsed(5000) && _stat_stop_watch.Update())
	{
		if ((_last_video_pts >= 0LL) && (_last_audio_pts >= 0LL))
		{
			auto video_pts_in_ms = static_cast<int64_t>(_last_video_pts * _video_timebase_expr_ms);
			auto audio_pts_in_ms = static_cast<int64_t>(_last_audio_pts * _audio_timebase_expr_ms);
			auto delta_in_ms = audio_pts_in_ms - video_pts_in_ms;

			logas("A-V Sync: %lldms (A: %lldms, V: %lldms)", delta_in_ms, audio_pts_in_ms, video_pts_in_ms);
		}
	}

	return true;
}

std::shared_ptr<const SegmentItem> DashPacketizer::GetSegmentData(const ov::String &file_name) const
{
	if (IsReadyForStreaming() == false)
	{
		logad("Could not get a segment %s because stream is not started", file_name.CStr());
		return nullptr;
	}

	auto file_type = GetFileType(file_name);

	switch (file_type)
	{
		case DashFileType::VideoInit:
			return _video_init_file;

		case DashFileType::AudioInit:
			return _audio_init_file;

		case DashFileType::VideoSegment:
			return _video_segment_queue.GetSegmentData(file_name);

		case DashFileType::AudioSegment:
			return _audio_segment_queue.GetSegmentData(file_name);

		default:
			break;
	}

	logad("Unknown file is requested: %s", file_name.CStr());
	return nullptr;
}

bool DashPacketizer::SetSegmentData(Writer &writer, int64_t timestamp)
{
	auto data = writer.GetData();
	auto duration = writer.GetDuration();
	auto media_type = writer.GetMediaType();

	writer.ResetData();

	if (data == nullptr)
	{
		logac("Data is null (%d)", static_cast<int>(media_type));
		OV_ASSERT2(false);
		return false;
	}

	switch (media_type)
	{
		case Writer::MediaType::Video: {
			auto file_name = GetFileName(_video_sequence_number, cmn::MediaType::Video);

			auto segment = _video_segment_queue.Append(
				SegmentDataType::Video, _video_sequence_number++,
				file_name,
				timestamp, timestamp * _video_timebase_expr_ms,
				duration, duration * _video_timebase_expr_ms,
				data);

			DumpSegmentToFile(segment);

			break;
		}

		case Writer::MediaType::Audio: {
			auto file_name = GetFileName(_audio_sequence_number, cmn::MediaType::Audio);

			auto segment = _audio_segment_queue.Append(
				SegmentDataType::Audio, _audio_sequence_number++,
				file_name,
				timestamp, timestamp * _audio_timebase_expr_ms,
				duration, duration * _audio_timebase_expr_ms,
				data);

			DumpSegmentToFile(segment);

			break;
		}

		default:
			logae("Could not set segment data for unknown media type: %d", static_cast<int>(media_type));
			return false;
	}

	if (IsReadyForStreaming() == false)
	{
		if (
			((_video_track == nullptr) || (_video_sequence_number >= _segment_count)) &&
			((_audio_track == nullptr) || (_audio_sequence_number >= _segment_count)))
		{
			SetReadyForStreaming();

			logai("Segments are ready to stream, segment duration: %fs, count: %u",
				  _segment_duration, _segment_count);
		}
	}

	return true;
}

void DashPacketizer::SetReadyForStreaming() noexcept
{
	if ((_video_start_time == -1L) || (_audio_start_time == -1L))
	{
		// Choose one of the two
		_start_time_ms = std::max(_video_start_time, _audio_start_time);
	}
	else
	{
		// Choose the time that came out faster between the two
		_start_time_ms = std::min(_video_start_time, _audio_start_time);
	}

	_start_time = ov::Time::MakeUtcMillisecond(_start_time_ms);

	Packetizer::SetReadyForStreaming();
}
