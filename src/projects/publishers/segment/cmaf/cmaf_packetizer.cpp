//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#include "cmaf_packetizer.h"
#include "cmaf_private.h"
// TODO(dimiden): Merge DASH and CMAF module later
#include "../dash/dash_define.h"

#include <algorithm>
#include <iomanip>
#include <numeric>
#include <sstream>

CmafPacketizer::CmafPacketizer(const ov::String &app_name, const ov::String &stream_name,
							   PacketizerStreamType stream_type,
							   const ov::String &segment_prefix,
							   uint32_t segment_count, uint32_t segment_duration,
							   std::shared_ptr<MediaTrack> video_track, std::shared_ptr<MediaTrack> audio_track,
							   const std::shared_ptr<ICmafChunkedTransfer> &chunked_transfer)
	: DashPacketizer(app_name, stream_name,
					 stream_type,
					 segment_prefix,
					 1, segment_duration,
					 video_track, audio_track)
{
	if (_video_track != nullptr)
	{
		_video_chunk_writer = std::make_shared<CmafChunkWriter>(M4sMediaType::Video, 1, _ideal_duration_for_video);
	}

	if (_audio_track != nullptr)
	{
		_audio_chunk_writer = std::make_shared<CmafChunkWriter>(M4sMediaType::Audio, 2, _ideal_duration_for_audio);
	}

	_chunked_transfer = chunked_transfer;
}

ov::String CmafPacketizer::GetFileName(int64_t start_timestamp, common::MediaType media_type) const
{
	// start_timestamp must be -1 because it is an unused parameter
	OV_ASSERT2(start_timestamp == -1LL);

	switch (media_type)
	{
		case common::MediaType::Video:
			return ov::String::FormatString("%s_%u%s", _segment_prefix.CStr(), _video_chunk_writer->GetSequenceNumber(), CMAF_MPD_VIDEO_FULL_SUFFIX);

		case common::MediaType::Audio:
			return ov::String::FormatString("%s_%u%s", _segment_prefix.CStr(), _audio_chunk_writer->GetSequenceNumber(), CMAF_MPD_AUDIO_FULL_SUFFIX);

		default:
			break;
	}

	return "";
}

bool CmafPacketizer::WriteVideoInit(const std::shared_ptr<ov::Data> &frame_data)
{
	return WriteVideoInitInternal(frame_data, CMAF_MPD_VIDEO_FULL_INIT_FILE_NAME);
}

bool CmafPacketizer::WriteAudioInit(const std::shared_ptr<ov::Data> &frame_data)
{
	return WriteAudioInitInternal(frame_data, CMAF_MPD_AUDIO_FULL_INIT_FILE_NAME);
}

bool CmafPacketizer::AppendVideoFrame(std::shared_ptr<PacketizerFrameData> &frame)
{
	return AppendVideoFrameInternal(frame, _video_chunk_writer->GetSegmentDuration(), [this, frame](const std::shared_ptr<const SampleData> data, bool new_segment_written) {
		auto chunk_data = _video_chunk_writer->AppendSample(data);

		if (chunk_data != nullptr && _chunked_transfer != nullptr)
		{
			// Response chunk data to HTTP client
			_chunked_transfer->OnCmafChunkDataPush(_app_name, _stream_name, GetFileName(-1LL, common::MediaType::Video), true, chunk_data);
		}

		_last_video_pts = data->timestamp;
	});
}

bool CmafPacketizer::AppendAudioFrame(std::shared_ptr<PacketizerFrameData> &frame)
{
	return AppendAudioFrameInternal(frame, _audio_chunk_writer->GetSegmentDuration(), [this, frame](const std::shared_ptr<const SampleData> data, bool new_segment_written) {
		auto chunk_data = _audio_chunk_writer->AppendSample(data);

		if (chunk_data != nullptr && _chunked_transfer != nullptr)
		{
			// Response chunk data to HTTP client
			_chunked_transfer->OnCmafChunkDataPush(_app_name, _stream_name, GetFileName(-1LL, common::MediaType::Audio), false, chunk_data);
		}

		_last_audio_pts = data->timestamp;
	});
}

bool CmafPacketizer::WriteVideoSegment()
{
	if (_video_chunk_writer->GetSampleCount() == 0)
	{
		logtd("There is no video data for CMAF segment");
		return true;
	}

	auto file_name = GetFileName(-1LL, common::MediaType::Video);
	auto start_timestamp = _video_chunk_writer->GetStartTimestamp();
	auto segment_duration = _video_chunk_writer->GetSegmentDuration();

	// Create a fragment
	auto segment_data = _video_chunk_writer->GetChunkedSegment();
	_video_chunk_writer->Clear();

	// Enqueue the segment
	if (SetSegmentData(file_name, segment_duration, start_timestamp, segment_data) == false)
	{
		return false;
	}

	_duration_delta_for_video += (_ideal_duration_for_video - segment_duration);

	if (_chunked_transfer != nullptr)
	{
		_chunked_transfer->OnCmafChunkedComplete(_app_name, _stream_name, file_name, true);
	}

	return true;
}

bool CmafPacketizer::WriteAudioSegment()
{
	if (_audio_chunk_writer->GetSampleCount() == 0)
	{
		logtd("There is no audio data for CMAF segment");
		return true;
	}

	auto file_name = GetFileName(-1LL, common::MediaType::Audio);
	auto start_timestamp = _audio_chunk_writer->GetStartTimestamp();
	auto segment_duration = _audio_chunk_writer->GetSegmentDuration();

	// Create a fragment
	auto segment_data = _audio_chunk_writer->GetChunkedSegment();
	_audio_chunk_writer->Clear();

	// Enqueue the segment
	if (SetSegmentData(file_name, segment_duration, start_timestamp, segment_data) == false)
	{
		return false;
	}

	_duration_delta_for_audio += (_ideal_duration_for_audio - segment_duration);

	if (_chunked_transfer != nullptr)
	{
		_chunked_transfer->OnCmafChunkedComplete(_app_name, _stream_name, file_name, false);
	}

	return true;
}

bool CmafPacketizer::UpdatePlayList()
{
	std::ostringstream play_list_stream;
	double time_shift_buffer_depth = 6;
	double minimumUpdatePeriod = 30;

	if (IsReadyForStreaming() == false)
	{
		return false;
	}

	ov::String publishTime = MakeUtcSecond(::time(nullptr));

	logtd("Trying to update playlist for CMAF with availabilityStartTime: %s, publishTime: %s", _start_time.CStr(), publishTime.CStr());

	play_list_stream
		<< std::fixed << std::setprecision(3)
		<< "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
		   "<MPD xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"\n"
		   "\txmlns=\"urn:mpeg:dash:schema:mpd:2011\"\n"
		   "\txmlns:xlink=\"http://www.w3.org/1999/xlink\"\n"
		   "\txsi:schemaLocation=\"urn:mpeg:DASH:schema:MPD:2011 http://standards.iso.org/ittf/PubliclyAvailableStandards/MPEG-DASH_schema_files/DASH-MPD.xsd\"\n"
		   "\tprofiles=\"urn:mpeg:dash:profile:isoff-live:2011\"\n"
		   "\ttype=\"dynamic\"\n"
		<< "\tminimumUpdatePeriod=\"PT" << minimumUpdatePeriod << "S\"\n"
		<< "\tpublishTime=\"" << publishTime.CStr() << "\"\n"
		<< "\tavailabilityStartTime=\"" << _start_time.CStr() << "\"\n"
		<< "\ttimeShiftBufferDepth=\"PT" << time_shift_buffer_depth << "S\"\n"
		<< "\tsuggestedPresentationDelay=\"PT" << _segment_duration << "S\"\n"
		<< "\tminBufferTime=\"PT" << _segment_duration << "S\">\n"
		<< "\t<Period id=\"0\" start=\"PT0S\">\n";

	if (_last_video_pts >= 0LL)
	{
		// availabilityTimeOffset
		//
		// - Proposed DASH extension
		// - Player usually sends request when entire segment available
		// - availabilityTimeOffset indicates difference of availabilityStartTime of the segment and UTC time
		//   of server when it can start delivering segment
		//
		// availabilityTimeOffset = [segment duration] - [chunk duration] - [UTC mismatch between server and client]

		// Reference: http://mile-high.video/files/mhv2018/pdf/day2/2_06_Henthorne.pdf

		double availability_time_offset = _video_track->GetFrameRate() != 0 ? (_segment_duration - (1.0 / _video_track->GetFrameRate())) : _segment_duration;

		play_list_stream
			<< "\t\t<AdaptationSet id=\"0\" group=\"1\" mimeType=\"video/mp4\" "
			<< "width=\"" << _video_track->GetWidth() << "\" height=\"" << _video_track->GetHeight()
			<< "\" par=\"" << _pixel_aspect_ratio << "\" frameRate=\"" << _video_track->GetFrameRate()
			<< "\" segmentAlignment=\"true\" startWithSAP=\"1\" subsegmentAlignment=\"true\" subsegmentStartsWithSAP=\"1\">\n"
			<< "\t\t\t<SegmentTemplate presentationTimeOffset=\"0\" timescale=\"" << static_cast<uint32_t>(_video_track->GetTimeBase().GetTimescale())
			<< "\" duration=\"" << static_cast<uint32_t>(_segment_duration * _video_track->GetTimeBase().GetTimescale())
			<< "\" availabilityTimeOffset=\"" << availability_time_offset
			<< "\" startNumber=\"0\" initialization=\"" << CMAF_MPD_VIDEO_FULL_INIT_FILE_NAME
			<< "\" media=\"" << _segment_prefix.CStr() << "_$Number$" << CMAF_MPD_VIDEO_FULL_SUFFIX << "\" />\n"
			<< "\t\t\t<Representation codecs=\"avc1.42401f\" sar=\"1:1\" "
			<< "bandwidth=\"" << _video_track->GetBitrate() << "\" />\n"
			<< "\t\t</AdaptationSet>\n";
	}

	if (_last_audio_pts >= 0LL)
	{
		// segment duration - audio one frame duration
		double availability_time_offset = (_audio_track->GetSampleRate() / 1024) != 0 ? _segment_duration - (1.0 / ((double)_audio_track->GetSampleRate() / 1024)) : _segment_duration;

		play_list_stream
			<< "\t\t<AdaptationSet id=\"1\" group=\"2\" mimeType=\"audio/mp4\" lang=\"und\" segmentAlignment=\"true\" "
			<< "startWithSAP=\"1\" subsegmentAlignment=\"true\" subsegmentStartsWithSAP=\"1\">\n"
			<< "\t\t\t<AudioChannelConfiguration schemeIdUri=\"urn:mpeg:dash:23003:3:audio_channel_configuration:2011\" "
			<< "value=\"" << _audio_track->GetChannel().GetCounts() << "\"/>\n"
			<< "\t\t\t<SegmentTemplate presentationTimeOffset=\"0\" timescale=\"" << static_cast<uint32_t>(_audio_track->GetTimeBase().GetTimescale())
			<< "\" duration=\"" << static_cast<uint32_t>(_segment_duration * _audio_track->GetTimeBase().GetTimescale())
			<< "\" availabilityTimeOffset=\"" << availability_time_offset
			<< "\" startNumber=\"0\" initialization=\"" << CMAF_MPD_AUDIO_FULL_INIT_FILE_NAME
			<< "\" media=\"" << _segment_prefix.CStr() << "_$Number$" << CMAF_MPD_AUDIO_FULL_SUFFIX << "\" />\n"
			<< "\t\t\t<Representation codecs=\"mp4a.40.2\" audioSamplingRate=\"" << _audio_track->GetSampleRate()
			<< "\" bandwidth=\"" << _audio_track->GetBitrate() << "\" />\n"
			<< "\t\t</AdaptationSet>\n";
	}

	play_list_stream << "\t</Period>\n"
					//  << "\t<UTCTiming schemeIdUri=\"urn:mpeg:dash:utc:direct:2014\" value=\"%s\"/>\n"
					 << "</MPD>\n";

	ov::String play_list = play_list_stream.str().c_str();

	SetPlayList(play_list);

	if(_stat_stop_watch.IsElapsed(5000) && _stat_stop_watch.Update())
	{
		if ((_last_video_pts >= 0LL) && (_last_audio_pts >= 0LL))
		{
			int64_t video_pts = static_cast<int64_t>(_last_video_pts * _video_track->GetTimeBase().GetExpr() * 1000.0);
			int64_t audio_pts = static_cast<int64_t>(_last_audio_pts * _audio_track->GetTimeBase().GetExpr() * 1000.0);

			logts("[%s/%s] LLDASH A-V Sync: %lld (A: %lld, V: %lld)",
				_app_name.CStr(), _stream_name.CStr(),
				audio_pts - video_pts, audio_pts, video_pts);
		}
	}

	return true;
}
