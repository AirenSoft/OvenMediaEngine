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
CmafStreamPacketizer::CmafStreamPacketizer(const ov::String &app_name,
										   const ov::String &stream_name,
										   int segment_count,
										   int segment_duration,
										   const ov::String &segment_prefix,
										   PacketizerStreamType stream_type,
										   std::shared_ptr<MediaTrack> video_track, std::shared_ptr<MediaTrack> audio_track,
										   const std::shared_ptr<ICmafChunkedTransfer> &chunked_transfer)
	: StreamPacketizer(app_name,
					   stream_name,
					   segment_count,
					   segment_duration,
					   stream_type,
					   video_track, audio_track)
{
	_packetizer = std::make_shared<CmafPacketizer>(app_name,
												   stream_name,
												   stream_type,
												   segment_prefix,
												   segment_count,
												   segment_duration,
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
bool CmafStreamPacketizer::AppendVideoFrame(std::shared_ptr<PacketizerFrameData> &data)
{
	return _packetizer->AppendVideoFrame(data);
}

//====================================================================================================
// Append Audi Frame
//====================================================================================================
bool CmafStreamPacketizer::AppendAudioFrame(std::shared_ptr<PacketizerFrameData> &data)
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
std::shared_ptr<SegmentData> CmafStreamPacketizer::GetSegmentData(const ov::String &file_name)
{
	return _packetizer->GetSegmentData(file_name);
}
