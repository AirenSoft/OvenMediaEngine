//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================

#include "dash_stream_server.h"
#include "dash_private.h"
#include "../segment_stream/packetyzer/packetyzer_define.h"

//====================================================================================================
// ProcessRequest URL
//====================================================================================================
void DashStreamServer::ProcessRequestStream(const std::shared_ptr<HttpResponse> &response,
                                            const ov::String &app_name,
                                            const ov::String &stream_name,
                                            const ov::String &file_name,
                                            const ov::String &file_ext)
{
	// request dispatch
	if (file_name == DASH_PLAYLIST_FILE_NAME)
		PlayListRequest(app_name, stream_name, file_name, PlayListType::Mpd, response);
	else if (file_ext == DASH_SEGMENT_EXT)
		SegmentRequest(app_name, stream_name, file_name, SegmentType::M4S, response);
	else
		response->SetStatusCode(HttpStatusCode::NotFound);// Error Response
}

//====================================================================================================
// PlayListRequest
//====================================================================================================
void DashStreamServer::PlayListRequest(const ov::String &app_name,
									   const ov::String &stream_name,
									   const ov::String &file_name,
									   PlayListType play_list_type,
									   const std::shared_ptr<HttpResponse> &response)
{
	ov::String play_list;

	auto item = std::find_if(_observers.begin(), _observers.end(),
							 [&app_name, &stream_name, &file_name, &play_list](
									 auto &observer) -> bool {
								 return observer->OnPlayListRequest(app_name, stream_name, file_name, play_list);
							 });

	if (item == _observers.end() || play_list.IsEmpty())
	{
		logtd("Dash PlayList Serarch Fail : %s/%s/%s", app_name.CStr(), stream_name.CStr(), file_name.CStr());
		response->SetStatusCode(HttpStatusCode::NotFound);
		return;
	}

	// header setting
	response->SetHeader("Content-Type", "application/dash+xml");

	response->AppendString(play_list);
}

//====================================================================================================
// SegmentRequest
//====================================================================================================
void DashStreamServer::SegmentRequest(const ov::String &app_name,
									  const ov::String &stream_name,
									  const ov::String &file_name,
									  SegmentType segment_type,
									  const std::shared_ptr<HttpResponse> &response)
{
	auto type = DashPacketyzer::GetFileType(file_name);
	bool is_video = (type == DashFileType::VideoSegment || type == DashFileType::VideoInit);

	std::shared_ptr<SegmentData> segment = nullptr;

	auto item = std::find_if(_observers.begin(), _observers.end(),
							 [&app_name, &stream_name, &file_name, &segment](
									 auto &observer) -> bool {
								 return observer->OnSegmentRequest(app_name, stream_name, file_name, segment);
							 });

	if (item == _observers.end())
	{
		logtd("Dash Segment Serarch Fail : %s/%s/%s", app_name.CStr(), stream_name.CStr(), file_name.CStr());
		response->SetStatusCode(HttpStatusCode::NotFound);
		return;
	}

	// header setting
	if(is_video)
		response->SetHeader("Content-Type", "video/mp4");
	else
		response->SetHeader("Content-Type", "audio/mp4");

	response->AppendData(segment->data);
}
