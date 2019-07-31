//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================

#include "dash_stream_server.h"
#include "../segment_stream/packetyzer/packetyzer_define.h"
#include "dash_private.h"

//====================================================================================================
// ProcessRequest URL
//====================================================================================================
void DashStreamServer::ProcessRequestStream(const std::shared_ptr<HttpResponse> &response,
											const ov::String &app_name,
											const ov::String &stream_name,
											const ov::String &file_name,
											const ov::String &file_ext)
{
	if (file_name == DASH_PLAYLIST_FILE_NAME)
	{
		OnPlayListRequest(app_name, stream_name, file_name, PlayListType::Mpd, response);
	}
	else if (file_ext == DASH_SEGMENT_EXT)
	{
		OnSegmentRequest(app_name, stream_name, file_name, SegmentType::M4S, response);
	}
	else
	{
		response->SetStatusCode(HttpStatusCode::NotFound);
	}
}

//====================================================================================================
// OnPlayListRequest
//====================================================================================================
void DashStreamServer::OnPlayListRequest(const ov::String &app_name,
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
		logtd("Could not find a DASH playlist for [%s/%s], %s", app_name.CStr(), stream_name.CStr(), file_name.CStr());
		response->SetStatusCode(HttpStatusCode::NotFound);
		return;
	}

	// Set HTTP header
	response->SetHeader("Content-Type", "application/dash+xml");

	response->AppendString(play_list);
}

//====================================================================================================
// OnSegmentRequest
//====================================================================================================
void DashStreamServer::OnSegmentRequest(const ov::String &app_name,
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
		logtd("Could not find a DASH segment for [%s/%s], %s", app_name.CStr(), stream_name.CStr(), file_name.CStr());
		response->SetStatusCode(HttpStatusCode::NotFound);
		return;
	}

	// Set HTTP header
	response->SetHeader("Content-Type", is_video ? "video/mp4" : "audio/mp4");

	response->AppendData(segment->data);
}
