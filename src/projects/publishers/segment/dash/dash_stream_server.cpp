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
#include "../segment_stream/time_interceptor.h"
#include "dash_define.h"
#include "dash_private.h"

std::shared_ptr<SegmentStreamInterceptor> DashStreamServer::CreateInterceptor()
{
	return std::make_shared<DashInterceptor>();
}

bool DashStreamServer::PrepareInterceptors(
	const std::shared_ptr<http::svr::HttpServer> &http_server,
	const std::shared_ptr<http::svr::HttpsServer> &https_server,
	int thread_count, const SegmentProcessHandler &process_handler)
{
	auto time_interceptor = std::make_shared<TimeInterceptor>();

	time_interceptor->Register(http::Method::All, R"(\/time)", [=](const std::shared_ptr<http::svr::HttpExchange> &client) -> http::svr::NextHandler {
		auto url = ov::Url::Parse(client->GetRequest()->GetUri());

		if (url == nullptr)
		{
			OV_ASSERT2(false);
			client->GetResponse()->SetStatusCode(http::StatusCode::InternalServerError);
			return http::svr::NextHandler::DoNotCall;
		}

		// (iso == false) && (ms == false) - unix timestamp
		// (iso == false) && (ms == true)  - unix timestamp + ms
		// (iso == true)  && (ms == false) - ISO8601 datetime
		// (iso == true)  && (ms == true)  - ISO8601 datetime + ms
		auto iso = url->HasQueryKey("iso");
		auto ms = url->HasQueryKey("ms");

		ov::String response_body;

		if (iso)
		{
			response_body = ms ? ov::Time::MakeUtcMillisecond() : ov::Time::MakeUtcSecond();
		}
		else
		{
			if (ms)
			{
				// 1621232660.123
				auto time_msec = ov::Time::GetTimestampInMs();
				response_body.Format("%d.%03d", (time_msec / 1000), (time_msec % 1000));
			}
			else
			{
				// 1621232660
				response_body = ov::Converter::ToString(ov::Time::GetTimestamp());
			}
		}

		auto response = client->GetResponse();

		response->SetHeader("Content-Type", "text/plain;charset=ISO-8859-1");
		response->SetHeader("Access-Control-Allow-Headers", "*");
		response->SetHeader("Access-Control-Allow-Methods", "*");
		response->SetHeader("Access-Control-Allow-Origin", "*");
		response->SetHeader("Access-Control-Expose-Headers", "Server,Content-Length,Date");

		response->AppendString(response_body);

		return http::svr::NextHandler::DoNotCall;
	});

	bool result = true;

	result = result && ((http_server == nullptr) || http_server->AddInterceptor(time_interceptor));
	result = result && ((https_server == nullptr) || https_server->AddInterceptor(time_interceptor));

	result = result && SegmentStreamServer::PrepareInterceptors(http_server, https_server, thread_count, process_handler);

	return result;
}

bool DashStreamServer::ProcessStreamRequest(const std::shared_ptr<http::svr::HttpExchange> &client,
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

	return true;
}

bool DashStreamServer::ProcessPlayListRequest(const std::shared_ptr<http::svr::HttpExchange> &client,
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
		logtd("[%s/%s] [%s] Could not find: %s",
			  request_info.vhost_app_name.CStr(), request_info.stream_name.CStr(), StringFromPublisherType(GetPublisherType()).CStr(),
			  request_info.file_name.CStr());
		response->SetStatusCode(http::StatusCode::NotFound);
		response->Response();

		return false;
	}

	if (response->GetStatusCode() != http::StatusCode::OK || play_list.IsEmpty())
	{
		response->Response();
		return false;
	}

	// Set HTTP header
	response->SetHeader("Content-Type", "application/dash+xml");
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

bool DashStreamServer::ProcessSegmentRequest(const std::shared_ptr<http::svr::HttpExchange> &client,
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
	response->SetHeader("Content-Type", (segment->type == SegmentDataType::Video) ? "video/mp4" : "audio/mp4");
	response->AppendData(segment->data);
	auto sent_bytes = response->Response();

	auto stream_info = GetStream(client);
	if (stream_info != nullptr)
	{
		MonitorInstance->IncreaseBytesOut(*stream_info, GetPublisherType(), sent_bytes);
	}

	return true;
}
