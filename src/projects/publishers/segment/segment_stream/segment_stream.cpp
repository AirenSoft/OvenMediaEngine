//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#include "segment_stream.h"
#include "config/items/items.h"
#include "segment_stream_private.h"
#include "stream_packetizer.h"

using namespace cmn;

SegmentStream::SegmentStream(const std::shared_ptr<pub::Application> application, const info::Stream &info)
	: Stream(application, info)
{
}

SegmentStream::~SegmentStream()
{
	Stop();
}

bool SegmentStream::Start(int segment_count, int segment_duration)
{
	std::shared_ptr<MediaTrack> video_track = nullptr;
	std::shared_ptr<MediaTrack> audio_track = nullptr;

	for (auto &track_item : _tracks)
	{
		auto &track = track_item.second;

		switch (track->GetMediaType())
		{
			case MediaType::Video:
				if (track->GetCodecId() == MediaCodecId::H264) // TODO(Dimiden): Need to support HEVC
				{
					video_track = track;
				}
				break;

			case MediaType::Audio:
				if (track->GetCodecId() == MediaCodecId::Aac)
				{
					audio_track = track;
				}
				break;

			default:
				break;
		}
	}

	bool video_enabled = false;
	bool audio_enabled = false;

	if ((video_track != nullptr) && (video_track->GetCodecId() == MediaCodecId::H264)) // // TODO(Dimiden): Need to support HEVC
	{
		_media_tracks[video_track->GetId()] = video_track;
		_video_track = video_track;
		video_enabled = true;
	}

	if ((audio_track != nullptr) && (audio_track->GetCodecId() == MediaCodecId::Aac))
	{
		_media_tracks[audio_track->GetId()] = audio_track;
		_audio_track = audio_track;
		audio_enabled = true;
	}

	if (video_enabled || audio_enabled)
	{
		PacketizerStreamType stream_type = PacketizerStreamType::Common;

		if (video_enabled == false)
		{
			stream_type = PacketizerStreamType::AudioOnly;
		}

		if (audio_enabled == false)
		{
			stream_type = PacketizerStreamType::VideoOnly;
		}

		_stream_packetizer = CreateStreamPacketizer(segment_count > 0 ? segment_count : DEFAULT_SEGMENT_COUNT,
													segment_duration > 0 ? segment_duration : DEFAULT_SEGMENT_DURATION,
													GetName(),  // stream name --> prefix
													stream_type,
													std::move(video_track), std::move(audio_track));
	}
	else
	{
		// log output
		//logtw("For output DASH/HLS, one of H264(video) or AAC(audio) codecs must be encoded.");
	}

	return Stream::Start();
}

bool SegmentStream::Stop()
{
	return Stream::Stop();
}

//====================================================================================================
// SendVideoFrame
// - Packetizer에 Video데이터 추가
// - 첫 key 프레임 에서 SPS/PPS 추출  이후 생성
//
//====================================================================================================
void SegmentStream::SendVideoFrame(const std::shared_ptr<MediaPacket> &media_packet)
{
	if (_stream_packetizer != nullptr && _media_tracks.find(media_packet->GetTrackId()) != _media_tracks.end())
	{
		//        int nul_header_size = 0;
		//
		//        if(fragmentation != nullptr && fragmentation->fragmentation_vector_size > 0)
		//        {
		//            nul_header_size = fragmentation->fragmentation_offset[fragmentation->fragmentation_vector_size - 1];
		//            logtd("null header size - %d", nul_header_size);
		//        }

		_stream_packetizer->AppendVideoData(media_packet, _video_track->GetTimeBase().GetTimescale());
	}
}

//====================================================================================================
// SendAudioFrame
// - Packetizer에 Audio데이터 추가
//====================================================================================================
void SegmentStream::SendAudioFrame(const std::shared_ptr<MediaPacket> &media_packet)
{
	if (_stream_packetizer != nullptr && _media_tracks.find(media_packet->GetTrackId()) != _media_tracks.end())
	{
		_stream_packetizer->AppendAudioData(media_packet, _audio_track->GetTimeBase().GetTimescale());
	}
}

//====================================================================================================
// GetPlayList
// - M3U8/MPD
//====================================================================================================
bool SegmentStream::GetPlayList(ov::String &play_list)
{
	if (_stream_packetizer != nullptr)
	{
		return _stream_packetizer->GetPlayList(play_list);
	}

	return false;
}

//====================================================================================================
// GetSegmentData
// - TS/M4S(mp4)
//====================================================================================================
std::shared_ptr<SegmentData> SegmentStream::GetSegmentData(const ov::String &file_name)
{
	if (_stream_packetizer == nullptr)
	{
		return nullptr;
	}

	return _stream_packetizer->GetSegmentData(file_name);
}