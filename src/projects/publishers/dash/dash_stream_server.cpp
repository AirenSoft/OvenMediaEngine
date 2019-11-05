//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#include "dash_stream_server.h"
#include "../dash/dash_define.h"
#include "../segment_stream/packetizer/packetizer_define.h"
#include "dash_private.h"

HttpConnection DashStreamServer::ProcessRequestStream(const std::shared_ptr<HttpClient> &client,
													  const ov::String &app_name, const ov::String &stream_name,
													  const ov::String &file_name, const ov::String &file_ext)
{
	auto &response = client->GetResponse();

	if (file_ext == DASH_PLAYLIST_EXT)
	{
		return OnPlayListRequest(client, app_name, stream_name, file_name, PlayListType::Mpd);
	}
	else if (file_ext == DASH_SEGMENT_EXT)
	{
		return OnSegmentRequest(client, app_name, stream_name, file_name, SegmentType::M4S);
	}

	response->SetStatusCode(HttpStatusCode::NotFound);
	response->Response();

	return HttpConnection::Closed;
}

HttpConnection DashStreamServer::OnPlayListRequest(const std::shared_ptr<HttpClient> &client,
												   const ov::String &app_name, const ov::String &stream_name,
												   const ov::String &file_name,
												   PlayListType play_list_type)
{
	auto &response = client->GetResponse();

	ov::String play_list;

	auto item = std::find_if(_observers.begin(), _observers.end(),
							 [&app_name, &stream_name, &file_name, &play_list](auto &observer) -> bool {
								 return observer->OnPlayListRequest(app_name, stream_name, file_name, play_list);
							 });

	if ((item == _observers.end()) || play_list.IsEmpty())
	{
		logtd("Could not find a %s playlist for [%s/%s], %s", GetPublisherName(), app_name.CStr(), stream_name.CStr(), file_name.CStr());
		response->SetStatusCode(HttpStatusCode::NotFound);
		response->Response();

		return HttpConnection::Closed;
	}

	// Set HTTP header
	response->SetHeader("Content-Type", "application/dash+xml");
	response->AppendString(play_list);
	response->Response();

	return HttpConnection::Closed;
}

HttpConnection DashStreamServer::OnSegmentRequest(const std::shared_ptr<HttpClient> &client,
												  const ov::String &app_name, const ov::String &stream_name,
												  const ov::String &file_name,
												  SegmentType segment_type)
{
	auto &response = client->GetResponse();

	std::shared_ptr<SegmentData> segment = nullptr;

	auto item = std::find_if(_observers.begin(), _observers.end(),
							 [&app_name, &stream_name, &file_name, &segment](auto &observer) -> bool {
								 return observer->OnSegmentRequest(app_name, stream_name, file_name, segment);
							 });

	if (item == _observers.end())
	{
		logtd("Could not find a %s segment for [%s/%s], %s", GetPublisherName(), app_name.CStr(), stream_name.CStr(), file_name.CStr());
		response->SetStatusCode(HttpStatusCode::NotFound);
		response->Response();
		
		return HttpConnection::Closed;
	}

	// Set HTTP header
	response->SetHeader("Content-Type", (segment->media_type == common::MediaType::Video) ? "video/mp4" : "audio/mp4");
	response->AppendData(segment->data);
	response->Response();

	return HttpConnection::Closed;
}
