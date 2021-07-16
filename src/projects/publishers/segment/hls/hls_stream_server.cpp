//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#include "hls_stream_server.h"

#include "../segment_publisher.h"
#include "hls_private.h"

http::svr::ConnectionPolicy HlsStreamServer::ProcessStreamRequest(const std::shared_ptr<http::svr::HttpConnection> &client,
													 const SegmentStreamRequestInfo &request_info,
													 const ov::String &file_ext)
{
	auto response = client->GetResponse();

	if (request_info.file_name == HLS_PLAYLIST_FILE_NAME)
	{
		return ProcessPlayListRequest(client, request_info, PlayListType::M3u8);
	}
	else if (file_ext == HLS_SEGMENT_EXT)
	{
		return ProcessSegmentRequest(client, request_info, SegmentType::MpegTs);
	}

	response->SetStatusCode(http::StatusCode::NotFound);
	response->Response();
	return http::svr::ConnectionPolicy::Closed;
}

http::svr::ConnectionPolicy HlsStreamServer::ProcessPlayListRequest(const std::shared_ptr<http::svr::HttpConnection> &client,
													   const SegmentStreamRequestInfo &request_info,
													   PlayListType play_list_type)
{
	auto response = client->GetResponse();

	ov::String play_list;

	auto item = std::find_if(_observers.begin(), _observers.end(),
							 [client, request_info, &play_list](std::shared_ptr<SegmentStreamObserver> &observer) -> bool {
								 return observer->OnPlayListRequest(client, request_info, play_list);
							 });

	if (item == _observers.end())
	{
		logtd("Could not find a %s playlist for [%s/%s], %s", GetPublisherName(), request_info.vhost_app_name.CStr(), request_info.stream_name.CStr(), request_info.file_name.CStr());
		response->SetStatusCode(http::StatusCode::NotFound);
		response->Response();

		return http::svr::ConnectionPolicy::Closed;
	}

	if (response->GetStatusCode() != http::StatusCode::OK || play_list.IsEmpty())
	{
		logte("Could not find a %s playlist for [%s/%s], %s : %d", GetPublisherName(), request_info.vhost_app_name.CStr(), request_info.stream_name.CStr(), request_info.file_name.CStr(), response->GetStatusCode());
		response->Response();
		return http::svr::ConnectionPolicy::Closed;
	}

	// Set HTTP header
	response->SetHeader("Content-Type", "application/vnd.apple.mpegurl");
	response->SetHeader("Cache-Control", "no-cache, no-store, must-revalidate");
	response->SetHeader("Pragma", "no-cache");
	response->SetHeader("Expires", "0");

	response->AppendString(play_list);
	auto sent_bytes = response->Response();

	auto stream_info = GetStream(client);
	if (stream_info != nullptr)
	{
		MonitorInstance->IncreaseBytesOut(*stream_info, GetPublisherType(), sent_bytes);
	}

	return http::svr::ConnectionPolicy::Closed;
}

http::svr::ConnectionPolicy HlsStreamServer::ProcessSegmentRequest(const std::shared_ptr<http::svr::HttpConnection> &client,
													  const SegmentStreamRequestInfo &request_info,
													  SegmentType segment_type)
{
	auto response = client->GetResponse();

	std::shared_ptr<const SegmentItem> segment = nullptr;

	auto item = std::find_if(_observers.begin(), _observers.end(),
							 [client, request_info, &segment](auto &observer) -> bool {
								 return observer->OnSegmentRequest(client, request_info, segment);
							 });

	if (item == _observers.end())
	{
		logtd("Could not find HLS segment: %s/%s, %s", request_info.vhost_app_name.CStr(), request_info.stream_name.CStr(), request_info.file_name.CStr());
		response->SetStatusCode(http::StatusCode::NotFound);
		response->Response();

		return http::svr::ConnectionPolicy::Closed;
	}

	// Set HTTP header
	response->SetHeader("Content-Type", "video/MP2T");
	response->AppendData(segment->data);
	auto sent_bytes = response->Response();

	auto stream_info = GetStream(client);
	if (stream_info != nullptr)
	{
		MonitorInstance->IncreaseBytesOut(*stream_info, GetPublisherType(), sent_bytes);
	}

	return http::svr::ConnectionPolicy::Closed;
}
