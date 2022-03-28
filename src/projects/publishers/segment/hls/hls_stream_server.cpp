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

bool HlsStreamServer::ProcessStreamRequest(const std::shared_ptr<http::svr::HttpExchange> &client,
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
	return false;
}

bool HlsStreamServer::ProcessPlayListRequest(const std::shared_ptr<http::svr::HttpExchange> &client,
																	const SegmentStreamRequestInfo &request_info,
																	PlayListType play_list_type)
{
	auto response = client->GetResponse();

	ov::String play_list;

	//TODO(h2) : Observer는 SegmentPublisher::OnPlayListRequest 이다. 
	// 여기서 SetExtra(stream)을 해서 stream을 넘겨준다. 
	auto item = std::find_if(_observers.begin(), _observers.end(),
							 [client, request_info, &play_list](std::shared_ptr<SegmentStreamObserver> &observer) -> bool {
								 return observer->OnPlayListRequest(client, request_info, play_list);
							 });

	if (item == _observers.end())
	{
		logtd("[%s/%s] [%s] Could not find: %s",
			  request_info.vhost_app_name.CStr(), request_info.stream_name.CStr(), StringFromPublisherType(GetPublisherType()).CStr(),
			  request_info.file_name.CStr());
		response->SetStatusCode(http::StatusCode::NotFound);
		response->Response();

		return false;
	}

	if (response->GetStatusCode() != http::StatusCode::OK || play_list.IsEmpty())
	{
		logte("[%s/%s] [%s] Could not find: %s (%d)",
			  request_info.vhost_app_name.CStr(), request_info.stream_name.CStr(), StringFromPublisherType(GetPublisherType()).CStr(),
			  request_info.file_name.CStr(), response->GetStatusCode());
		response->Response();
		return false;
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

	return true;
}

bool HlsStreamServer::ProcessSegmentRequest(const std::shared_ptr<http::svr::HttpExchange> &client,
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
		logtd("[%s/%s] [%s] Could not find: %s",
			  request_info.vhost_app_name.CStr(), request_info.stream_name.CStr(), StringFromPublisherType(GetPublisherType()).CStr(),
			  request_info.file_name.CStr());
		response->SetStatusCode(http::StatusCode::NotFound);
		response->Response();

		return false;
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

	return true;
}
