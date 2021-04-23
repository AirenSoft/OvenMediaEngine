//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#include "dash_stream_server.h"

#include <monitoring/monitoring.h>
#include <publishers/segment/segment_stream/packetizer/packetizer_define.h>

#include "../segment_publisher.h"
#include "dash_define.h"
#include "dash_private.h"

http::svr::ConnectionPolicy DashStreamServer::ProcessStreamRequest(const std::shared_ptr<http::svr::HttpConnection> &client,
													  const SegmentStreamRequestInfo &request_info,
													  const ov::String &file_ext)
{
	auto response = client->GetResponse();

	if (file_ext == DASH_PLAYLIST_EXT)
	{
		return ProcessPlayListRequest(client, request_info, PlayListType::Mpd);
	}
	else if (file_ext == DASH_SEGMENT_EXT)
	{
		return ProcessSegmentRequest(client, request_info, SegmentType::M4S);
	}

	response->SetStatusCode(http::StatusCode::NotFound);
	response->Response();

	return http::svr::ConnectionPolicy::Closed;
}

http::svr::ConnectionPolicy DashStreamServer::ProcessPlayListRequest(const std::shared_ptr<http::svr::HttpConnection> &client,
														const SegmentStreamRequestInfo &request_info,
														PlayListType play_list_type)
{
	auto response = client->GetResponse();

	ov::String play_list;

	auto item = std::find_if(_observers.begin(), _observers.end(),
							 [client, request_info, &play_list](std::shared_ptr<SegmentStreamObserver> &observer) -> bool {
								 return observer->OnPlayListRequest(client, request_info, play_list);
							 });

	if ((item == _observers.end()))
	{
		logtd("Could not find a %s playlist for [%s/%s], %s", GetPublisherName(), request_info.vhost_app_name.CStr(), request_info.stream_name.CStr(), request_info.file_name.CStr());
		response->SetStatusCode(http::StatusCode::NotFound);
		response->Response();

		return http::svr::ConnectionPolicy::Closed;
	}

	if (response->GetStatusCode() != http::StatusCode::OK || play_list.IsEmpty())
	{
		response->Response();
		return http::svr::ConnectionPolicy::Closed;
	}

	// Set HTTP header
	response->SetHeader("Content-Type", "application/dash+xml");
	response->SetHeader("Cache-Control", "no-cache, no-store, must-revalidate");
	response->SetHeader("Pragma", "no-cache");
	response->SetHeader("Expires", "0");

	response->AppendString(play_list);
	auto sent_bytes = response->Response();

	auto metric = GetStreamMetric(client);
	if(metric != nullptr)
	{
		metric->IncreaseBytesOut(GetPublisherType(), sent_bytes);
	}

	return http::svr::ConnectionPolicy::Closed;
}

http::svr::ConnectionPolicy DashStreamServer::ProcessSegmentRequest(const std::shared_ptr<http::svr::HttpConnection> &client,
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
		logtd("Could not find a %s segment for [%s/%s], %s", GetPublisherName(), request_info.vhost_app_name.CStr(), request_info.stream_name.CStr(), request_info.file_name.CStr());
		response->SetStatusCode(http::StatusCode::NotFound);
		response->Response();

		return http::svr::ConnectionPolicy::Closed;
	}

	// Set HTTP header
	response->SetHeader("Content-Type", (segment->type == SegmentDataType::Video) ? "video/mp4" : "audio/mp4");
	response->AppendData(segment->data);
	auto sent_bytes = response->Response();

	auto metric = GetStreamMetric(client);
	if(metric != nullptr)
	{
		metric->IncreaseBytesOut(GetPublisherType(), sent_bytes);
	}


	return http::svr::ConnectionPolicy::Closed;
}
