//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#include "hls_stream_server.h"

#include <monitoring/monitoring.h>

#include "../segment_publisher.h"
#include "hls_private.h"

HttpConnection HlsStreamServer::ProcessStreamRequest(const std::shared_ptr<HttpClient> &client,
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

	response->SetStatusCode(HttpStatusCode::NotFound);
	response->Response();
	return HttpConnection::Closed;
}

HttpConnection HlsStreamServer::ProcessPlayListRequest(const std::shared_ptr<HttpClient> &client,
													   const SegmentStreamRequestInfo &request_info,
													   PlayListType play_list_type)
{
	auto response = client->GetResponse();

	ov::String play_list;
	std::shared_ptr<info::Stream> stream_info;

	auto item = std::find_if(_observers.begin(), _observers.end(),
							 [client, request_info, &play_list, &stream_info](std::shared_ptr<SegmentStreamObserver> &observer) -> bool {
								 std::shared_ptr<SegmentPublisher> publisher = std::static_pointer_cast<SegmentPublisher>(observer);
								 if(observer->OnPlayListRequest(client, request_info, play_list))
								 {
									stream_info = publisher->GetStreamAs<info::Stream>(request_info.vhost_app_name, request_info.stream_name);
									return true;
								 }

								 return false;
							 });

	if (item == _observers.end())
	{
		logtd("Could not find a %s playlist for [%s/%s], %s", GetPublisherName(), request_info.vhost_app_name.CStr(), request_info.stream_name.CStr(), request_info.file_name.CStr());
		response->SetStatusCode(HttpStatusCode::NotFound);
		response->Response();

		return HttpConnection::Closed;
	}

	if (response->GetStatusCode() != HttpStatusCode::OK || play_list.IsEmpty())
	{
		logte("Could not find a %s playlist for [%s/%s], %s : %d", GetPublisherName(), request_info.vhost_app_name.CStr(), request_info.stream_name.CStr(), request_info.file_name.CStr(), response->GetStatusCode());
		response->Response();
		return HttpConnection::Closed;
	}

	// Set HTTP header
	response->SetHeader("Content-Type", "application/vnd.apple.mpegurl");
	response->SetHeader("Cache-Control", "no-cache, no-store, must-revalidate");
	response->SetHeader("Pragma", "no-cache");
	response->SetHeader("Expires", "0");

	response->AppendString(play_list);
	auto sent_bytes = response->Response();

	if (stream_info != nullptr)
	{
		auto stream_metric = StreamMetrics(*stream_info);
		if (stream_metric != nullptr)
		{
			stream_metric->IncreaseBytesOut(PublisherType::Hls, sent_bytes);
		}
	}

	return HttpConnection::Closed;
}

HttpConnection HlsStreamServer::ProcessSegmentRequest(const std::shared_ptr<HttpClient> &client,
													  const SegmentStreamRequestInfo &request_info,
													  SegmentType segment_type)
{
	auto response = client->GetResponse();

	std::shared_ptr<SegmentData> segment = nullptr;
	std::shared_ptr<info::Stream> stream_info;

	auto item = std::find_if(_observers.begin(), _observers.end(),
							 [client, request_info, &segment, &stream_info](auto &observer) -> bool {
								 auto publisher = std::static_pointer_cast<SegmentPublisher>(observer);
								 auto stream = publisher->GetStream(request_info.vhost_app_name, request_info.stream_name);
								 stream_info = std::static_pointer_cast<info::Stream>(stream);
								 return observer->OnSegmentRequest(client, request_info, segment);
							 });

	if (item == _observers.end())
	{
		logtd("Could not find HLS segment: %s/%s, %s", request_info.vhost_app_name.CStr(), request_info.stream_name.CStr(), request_info.file_name.CStr());
		response->SetStatusCode(HttpStatusCode::NotFound);
		response->Response();

		return HttpConnection::Closed;
	}

	// Set HTTP header
	response->SetHeader("Content-Type", "video/MP2T");
	response->AppendData(segment->data);
	auto sent_bytes = response->Response();

	if (stream_info != nullptr)
	{
		auto stream_metric = StreamMetrics(*stream_info);
		if (stream_metric != nullptr)
		{
			stream_metric->IncreaseBytesOut(PublisherType::Hls, sent_bytes);
		}
	}

	return HttpConnection::Closed;
}
