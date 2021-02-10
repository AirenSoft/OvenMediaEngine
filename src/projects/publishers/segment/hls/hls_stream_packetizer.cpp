//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#include "hls_stream_packetizer.h"

#include "hls_private.h"

HlsStreamPacketizer::HlsStreamPacketizer(const ov::String &app_name, const ov::String &stream_name,
										 int segment_count, int segment_duration,
										 std::shared_ptr<MediaTrack> video_track, std::shared_ptr<MediaTrack> audio_track,
										 const std::shared_ptr<ChunkedTransferInterface> &chunked_transfer)
	: StreamPacketizer(app_name, stream_name,
					   segment_count, segment_duration,
					   video_track, audio_track,
					   chunked_transfer)
{
	_packetizer = std::make_shared<HlsPacketizer>(app_name, stream_name,
												  segment_count, segment_duration,
												  video_track, audio_track,
												  chunked_transfer);
}

bool HlsStreamPacketizer::AppendVideoData(const std::shared_ptr<MediaPacket> &media_packet)
{
	return _packetizer->AppendVideoFrame(media_packet);
}

bool HlsStreamPacketizer::AppendAudioData(const std::shared_ptr<MediaPacket> &media_packet)
{
	return _packetizer->AppendAudioFrame(media_packet);
}

bool HlsStreamPacketizer::GetPlayList(ov::String &play_list)
{
	return _packetizer->GetPlayList(play_list);
}

std::shared_ptr<const SegmentItem> HlsStreamPacketizer::GetSegmentData(const ov::String &file_name) const
{
	return _packetizer->GetSegmentData(file_name);
}
