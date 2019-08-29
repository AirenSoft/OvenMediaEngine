//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================

#include "hls_stream_server.h"
#include "hls_private.h"

void HlsStreamServer::ProcessRequestStream(const std::shared_ptr<HttpResponse> &response,
										   const ov::String &app_name, const ov::String &stream_name,
										   const ov::String &file_name, const ov::String &file_ext)
{
	if (file_name == HLS_PLAYLIST_FILE_NAME)
	{
		OnPlayListRequest(app_name, stream_name, file_name, PlayListType::M3u8, response);
	}
	else if (file_ext == HLS_SEGMENT_EXT)
	{
		OnSegmentRequest(app_name, stream_name, file_name, SegmentType::MpegTs, response);
	}
	else
	{
		// Response error
		response->SetStatusCode(HttpStatusCode::NotFound);
	}
}

void HlsStreamServer::OnPlayListRequest(const ov::String &app_name, const ov::String &stream_name,
										const ov::String &file_name,
										PlayListType play_list_type,
										const std::shared_ptr<HttpResponse> &response)
{
	ov::String play_list;

	auto item = std::find_if(_observers.begin(), _observers.end(),
							 [&app_name, &stream_name, &file_name, &play_list](auto &observer) -> bool {
								 return observer->OnPlayListRequest(app_name, stream_name, file_name, play_list);
							 });

	if (play_list.IsEmpty())
	{
		logtd("Could not find HLS playlist: %s/%s, %s", app_name.CStr(), stream_name.CStr(), file_name.CStr());
		response->SetStatusCode(HttpStatusCode::NotFound);
		return;
	}

	// Set HTTP header
	response->SetHeader("Content-Type", "application/x-mpegURL");

	response->AppendString(play_list);
}

void HlsStreamServer::OnSegmentRequest(const ov::String &app_name, const ov::String &stream_name,
									   const ov::String &file_name,
									   SegmentType segment_type,
									   const std::shared_ptr<HttpResponse> &response)
{
	std::shared_ptr<SegmentData> segment = nullptr;

	auto item = std::find_if(_observers.begin(), _observers.end(),
							 [&app_name, &stream_name, &file_name, &segment](auto &observer) -> bool {
								 return observer->OnSegmentRequest(app_name, stream_name, file_name, segment);
							 });

	if (item == _observers.end())
	{
		logtd("Could not find HLS segment: %s/%s, %s", app_name.CStr(), stream_name.CStr(), file_name.CStr());
		response->SetStatusCode(HttpStatusCode::NotFound);
		return;
	}

	// Set HTTP header
	response->SetHeader("Content-Type", "video/MP2T");

	response->AppendData(segment->data);
}
