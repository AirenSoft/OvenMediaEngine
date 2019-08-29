//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================

#include "cmaf_stream_packetyzer.h"
#include "cmaf_private.h"

//====================================================================================================
// Constructor
//====================================================================================================
CmafStreamPacketyzer::CmafStreamPacketyzer(const ov::String &app_name,
										   const ov::String &stream_name,
										   int segment_count,
										   int segment_duration,
										   const ov::String &segment_prefix,
										   PacketyzerStreamType stream_type,
										   std::shared_ptr<MediaTrack> video_track, std::shared_ptr<MediaTrack> audio_track,
										   const std::shared_ptr<ICmafChunkedTransfer> &chunked_transfer)
	: StreamPacketyzer(app_name,
					   stream_name,
					   segment_count,
					   segment_duration,
					   stream_type,
					   video_track, audio_track)
{
	_packetyzer = std::make_shared<CmafPacketyzer>(app_name,
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
CmafStreamPacketyzer::~CmafStreamPacketyzer()
{
}

//====================================================================================================
// Append Video Frame
//====================================================================================================
bool CmafStreamPacketyzer::AppendVideoFrame(std::shared_ptr<PacketyzerFrameData> &data)
{
	return _packetyzer->AppendVideoFrame(data);
}

//====================================================================================================
// Append Audi Frame
//====================================================================================================
bool CmafStreamPacketyzer::AppendAudioFrame(std::shared_ptr<PacketyzerFrameData> &data)
{
	return _packetyzer->AppendAudioFrame(data);
}

//====================================================================================================
// Get PlayList
// - MPD
//====================================================================================================
bool CmafStreamPacketyzer::GetPlayList(ov::String &play_list)
{
	return _packetyzer->GetPlayList(play_list);
}

//====================================================================================================
// GetSegmentData
// - M4S
//====================================================================================================
std::shared_ptr<SegmentData> CmafStreamPacketyzer::GetSegmentData(const ov::String &file_name)
{
	return _packetyzer->GetSegmentData(file_name);
}
