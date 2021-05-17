//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#include "packetizer.h"

#include <modules/bitstream/aac/aac_converter.h>
#include <modules/bitstream/h264/h264_converter.h>

#include <algorithm>
#include <sstream>

#include "../segment_stream_private.h"

Packetizer::Packetizer(const ov::String &app_name, const ov::String &stream_name,
					   uint32_t segment_count, uint32_t segment_save_count, uint32_t segment_duration,
					   const std::shared_ptr<MediaTrack> &video_track, const std::shared_ptr<MediaTrack> &audio_track,
					   const std::shared_ptr<ChunkedTransferInterface> &chunked_transfer)

	: _app_name(app_name), _stream_name(stream_name),

	  _segment_count(segment_count),
	  _segment_save_count(segment_save_count),
	  _segment_duration(segment_duration),

	  _video_track(video_track),
	  _audio_track(audio_track),

	  _chunked_transfer(chunked_transfer)
{
	_video_segments.resize(_segment_save_count);
	_audio_segments.resize(_segment_save_count);
}

uint64_t Packetizer::ConvertTimeScale(uint64_t time, const cmn::Timebase &from_timebase, const cmn::Timebase &to_timebase)
{
	if (from_timebase.GetExpr() == 0.0)
	{
		return 0;
	}

	double ratio = from_timebase.GetExpr() / to_timebase.GetExpr();

	return (uint64_t)((double)time * ratio);
}

void Packetizer::SetPlayList(const ov::String &play_list)
{
	std::unique_lock<std::mutex> lock(_play_list_mutex);

	_play_list = play_list;
}

bool Packetizer::IsReadyForStreaming() const noexcept
{
	return _streaming_start;
}

void Packetizer::SetReadyForStreaming() noexcept
{
	_streaming_start = true;
}

ov::String Packetizer::GetCodecString(const std::shared_ptr<const MediaTrack> &track)
{
	ov::String codec_string;

	switch (track->GetCodecId())
	{
		case cmn::MediaCodecId::None:
			// Not supported
			break;

		case cmn::MediaCodecId::H264: 
		{
			auto profile_string = H264Converter::GetProfileString(track->GetCodecExtradata());
			if (profile_string.IsEmpty())
			{
				profile_string = H264_CONVERTER_DEFAULT_PROFILE;
			}

			codec_string.AppendFormat("avc1.%s", profile_string.CStr());

			break;
		}

		case cmn::MediaCodecId::H265: {
			// https://developer.mozilla.org/en-US/docs/Web/Media/Formats/codecs_parameter#ISO_Base_Media_File_Format_syntax
			//
			// cccc.PP.LL.DD.CC[.cp[.tc[.mc[.FF]]]]
			//
			// cccc = FOURCC
			// PP = Profile
			// LL = Level
			// DD = Bit Depth
			// CC = Chroma Subsampling
			// cp = Color Primaries
			// tc = Transfer Characteristics
			// mc = Matrix Coefficients
			// FF = Indicates whether to restrict tlack level and color range of each color
			codec_string = "hev1.1.6.L93.B0";
			break;
		}

		case cmn::MediaCodecId::Vp8: {
			// Not supported (vp08.00.41.08)
			break;
		}

		case cmn::MediaCodecId::Vp9: {
			// Not supported (vp09.02.10.10.01.09.16.09.01)
			break;
		}

		case cmn::MediaCodecId::Flv: {
			// Not supported
			break;
		}

		case cmn::MediaCodecId::Aac: {
			auto profile_string = AacConverter::GetProfileString(track->GetAacConfig());

			if (profile_string.IsEmpty())
			{
				profile_string = AAC_CONVERTER_DEFAULT_PROFILE;
			}

			// "mp4a.oo[.A]"
			//   oo = OTI = [Object Type Indication]
			//   A = [Object Type Number]
			//
			// OTI == 40 (Audio ISO/IEC 14496-3)
			//   http://mp4ra.org/#/object_types
			//
			// OTN == profile_number
			//   https://developer.mozilla.org/en-US/docs/Web/Media/Formats/codecs_parameter#MPEG-4_audio
			codec_string.AppendFormat("mp4a.40.%s", profile_string.CStr());

			break;
		}

		case cmn::MediaCodecId::Mp3:
			// OTI == 40 (Audio ISO/IEC 14496-3)
			//   http://mp4ra.org/#/object_types
			//
			// OTN == 34 (MPEG-1 Layer-3 (MP3))
			//   https://developer.mozilla.org/en-US/docs/Web/Media/Formats/codecs_parameter#MPEG-4_audio
			codec_string.AppendFormat("mp4a.40.34");
			break;

		case cmn::MediaCodecId::Opus:
			// http://mp4ra.org/#/object_types
			// OTI == ad (Opus audio)
			codec_string.AppendFormat("mp4a.ad");
			break;

		case cmn::MediaCodecId::Jpeg: {
			// Not supported
			break;
		}

		case cmn::MediaCodecId::Png: {
			// Not supported
			break;
		}
	}

	return codec_string;
}

bool Packetizer::GetPlayList(ov::String &play_list)
{
	if (IsReadyForStreaming() == false)
	{
		logtd("Manifest was requested before the stream began");
		return false;
	}

	std::unique_lock<std::mutex> lock(_play_list_mutex);
	play_list = _play_list;

	return true;
}

bool Packetizer::GetVideoPlaySegments(std::vector<std::shared_ptr<SegmentItem>> &segment_datas)
{
	uint32_t begin_index = (_current_video_index >= _segment_count) ? (_current_video_index - _segment_count) : (_segment_save_count - (_segment_count - _current_video_index));
	uint32_t end_index = (begin_index <= (_segment_save_count - _segment_count)) ? (begin_index + _segment_count) - 1 : (_segment_count - (_segment_save_count - begin_index)) - 1;

	// video segment mutex
	std::unique_lock<std::mutex> lock(_video_segment_mutex);

	if (begin_index <= end_index)
	{
		for (auto item = _video_segments.begin() + begin_index; item <= _video_segments.begin() + end_index; item++)
		{
			if (*item == nullptr)
			{
				return true;
			}

			segment_datas.push_back(*item);
		}
	}
	else
	{
		for (auto item = _video_segments.begin() + begin_index; item < _video_segments.end(); item++)
		{
			if (*item == nullptr)
			{
				return true;
			}

			segment_datas.push_back(*item);
		}

		for (auto item = _video_segments.begin(); item <= _video_segments.begin() + end_index; item++)
		{
			if (*item == nullptr)
			{
				return true;
			}

			segment_datas.push_back(*item);
		}
	}

	return true;
}

bool Packetizer::GetAudioPlaySegments(std::vector<std::shared_ptr<SegmentItem>> &segment_datas)
{
	uint32_t begin_index = (_current_audio_index >= _segment_count) ? (_current_audio_index - _segment_count) : (_segment_save_count - (_segment_count - _current_audio_index));
	uint32_t end_index = (begin_index <= (_segment_save_count - _segment_count)) ? (begin_index + _segment_count) - 1 : (_segment_count - (_segment_save_count - begin_index)) - 1;

	// audio segment mutex
	std::unique_lock<std::mutex> lock(_audio_segment_mutex);

	if (begin_index <= end_index)
	{
		for (auto item = _audio_segments.begin() + begin_index; item <= _audio_segments.begin() + end_index; item++)
		{
			if (*item == nullptr)
			{
				return true;
			}

			segment_datas.push_back(*item);
		}
	}
	else
	{
		for (auto item = _audio_segments.begin() + begin_index; item < _audio_segments.end(); item++)
		{
			if (*item == nullptr)
			{
				return true;
			}

			segment_datas.push_back(*item);
		}
		for (auto item = _audio_segments.begin(); item <= _audio_segments.begin() + end_index; item++)
		{
			if (*item == nullptr)
			{
				return true;
			}

			segment_datas.push_back(*item);
		}
	}

	return true;
}