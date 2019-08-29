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
#include "stream_packetyzer.h"

using namespace common;

SegmentStream::SegmentStream(const std::shared_ptr<Application> application, const StreamInfo &info)
	: Stream(application, info)
{
}

SegmentStream::~SegmentStream()
{
	Stop();
}

bool SegmentStream::Start(int segment_count, int segment_duration, uint32_t worker_count)
{
	std::shared_ptr<MediaTrack> video_track = nullptr;
	std::shared_ptr<MediaTrack> audio_track = nullptr;

	for (auto &track_item : _tracks)
	{
		auto &track = track_item.second;

		switch (track->GetMediaType())
		{
			case MediaType::Video:
				if (track->GetCodecId() == MediaCodecId::H264)
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
		}
	}

	if ((video_track != nullptr) && (video_track->GetCodecId() == MediaCodecId::H264))
	{
		_media_tracks[video_track->GetId()] = video_track;
		_video_track = video_track;
	}

	if ((audio_track != nullptr) && (audio_track->GetCodecId() == MediaCodecId::Aac))
	{
		_media_tracks[audio_track->GetId()] = audio_track;
		_audio_track = audio_track;
	}

	if ((video_track != nullptr) || (audio_track != nullptr))
	{
		PacketyzerStreamType stream_type = PacketyzerStreamType::Common;

		if (video_track == nullptr)
		{
			stream_type = PacketyzerStreamType::AudioOnly;
		}

		if (audio_track == nullptr)
		{
			stream_type = PacketyzerStreamType::VideoOnly;
		}

		_stream_packetyzer = CreateStreamPacketyzer(segment_count > 0 ? segment_count : DEFAULT_SEGMENT_COUNT,
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

	return Stream::Start(worker_count);
}

bool SegmentStream::Stop()
{
	return Stream::Stop();
}

//====================================================================================================
// SendVideoFrame
// - Packetyzer에 Video데이터 추가
// - 첫 key 프레임 에서 SPS/PPS 추출  이후 생성
//
//====================================================================================================
void SegmentStream::SendVideoFrame(std::shared_ptr<MediaTrack> track,
								   std::unique_ptr<EncodedFrame> encoded_frame,
								   std::unique_ptr<CodecSpecificInfo> codec_info,
								   std::unique_ptr<FragmentationHeader> fragmentation)
{
	if (_stream_packetyzer != nullptr && _media_tracks.find(track->GetId()) != _media_tracks.end())
	{
		//        int nul_header_size = 0;
		//
		//        if(fragmentation != nullptr && fragmentation->fragmentation_vector_size > 0)
		//        {
		//            nul_header_size = fragmentation->fragmentation_offset[fragmentation->fragmentation_vector_size - 1];
		//            logtd("null header size - %d", nul_header_size);
		//        }

		_stream_packetyzer->AppendVideoData(std::move(encoded_frame), _video_track->GetTimeBase().GetTimescale(), 0);
	}
}

//====================================================================================================
// SendAudioFrame
// - Packetyzer에 Audio데이터 추가
//====================================================================================================
void SegmentStream::SendAudioFrame(std::shared_ptr<MediaTrack> track,
								   std::unique_ptr<EncodedFrame> encoded_frame,
								   std::unique_ptr<CodecSpecificInfo> codec_info,
								   std::unique_ptr<FragmentationHeader> fragmentation)
{
	if (_stream_packetyzer != nullptr && _media_tracks.find(track->GetId()) != _media_tracks.end())
	{
		_stream_packetyzer->AppendAudioData(std::move(encoded_frame), _audio_track->GetTimeBase().GetTimescale());
	}
}

//====================================================================================================
// GetPlayList
// - M3U8/MPD
//====================================================================================================
bool SegmentStream::GetPlayList(ov::String &play_list)
{
	if (_stream_packetyzer != nullptr)
	{
		return _stream_packetyzer->GetPlayList(play_list);
	}

	return false;
}

//====================================================================================================
// GetSegmentData
// - TS/M4S(mp4)
//====================================================================================================
std::shared_ptr<SegmentData> SegmentStream::GetSegmentData(const ov::String &file_name)
{
	if (_stream_packetyzer == nullptr)
		return nullptr;

	return _stream_packetyzer->GetSegmentData(file_name);
}