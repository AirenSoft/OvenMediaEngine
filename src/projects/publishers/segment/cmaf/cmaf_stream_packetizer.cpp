//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================

#include "cmaf_stream_packetizer.h"

#include "cmaf_private.h"

//====================================================================================================
// Constructor
//====================================================================================================
CmafStreamPacketizer::CmafStreamPacketizer(const ov::String &app_name, const ov::String &stream_name,
										   int segment_count, int segment_duration,
										   std::shared_ptr<MediaTrack> video_track, std::shared_ptr<MediaTrack> audio_track,
										   const std::shared_ptr<ChunkedTransferInterface> &chunked_transfer)
	: StreamPacketizer(app_name, stream_name,
					   segment_count, segment_duration,
					   video_track, audio_track,
					   chunked_transfer)
{
	_packetizer = std::make_shared<CmafPacketizer>(app_name, stream_name,
												   segment_count, segment_duration,
												   video_track, audio_track,
												   chunked_transfer);
}

//====================================================================================================
// Destructor
//====================================================================================================
CmafStreamPacketizer::~CmafStreamPacketizer()
{
}

//====================================================================================================
// Append Video Frame
//====================================================================================================
bool CmafStreamPacketizer::AppendVideoFrame(const std::shared_ptr<const PacketizerFrameData> &data)
{
	return _packetizer->AppendVideoFrame(data);
}

//====================================================================================================
// Append Audi Frame
//====================================================================================================
bool CmafStreamPacketizer::AppendAudioFrame(const std::shared_ptr<const PacketizerFrameData> &data)
{
	return _packetizer->AppendAudioFrame(data);
}

//====================================================================================================
// Get PlayList
// - MPD
//====================================================================================================
bool CmafStreamPacketizer::GetPlayList(ov::String &play_list)
{
	return _packetizer->GetPlayList(play_list);
}

//====================================================================================================
// GetSegmentData
// - M4S
//====================================================================================================
std::shared_ptr<const SegmentItem> CmafStreamPacketizer::GetSegmentData(const ov::String &file_name) const
{
	return _packetizer->GetSegmentData(file_name);
}
