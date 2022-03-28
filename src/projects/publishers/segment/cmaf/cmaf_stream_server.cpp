//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#include "cmaf_stream_server.h"

#include <monitoring/monitoring.h>

#include <modules/http/server/http1/http1_response.h>

#include "../dash/dash_define.h"
#include "../segment_publisher.h"
#include "cmaf_packetizer.h"
#include "cmaf_private.h"

std::shared_ptr<SegmentStreamInterceptor> CmafStreamServer::CreateInterceptor()
{
	return std::make_shared<CmafInterceptor>();
}

bool CmafStreamServer::ProcessSegmentRequest(const std::shared_ptr<http::svr::HttpExchange> &client,
																	const SegmentStreamRequestInfo &request_info,
																	SegmentType segment_type)
{
	// Cast to HTTP/1.1 Response
	auto response = std::static_pointer_cast<http::svr::h1::Http1Response>(client->GetResponse());
	if (response == nullptr)
	{
		logte("LLDASH only supports HTTP/1.1.");
		return false;
	}

	auto type = CmafPacketizer::GetFileType(request_info.file_name);

	bool is_video = ((type == DashFileType::VideoSegment) || (type == DashFileType::VideoInit));

	// Check if the requested file is being created
	{
		auto key = ov::String::FormatString("%s/%s/%s", request_info.vhost_app_name.CStr(), request_info.stream_name.CStr(), request_info.file_name.CStr());

		std::unique_lock<std::mutex> lock(_http_chunk_guard);

		auto chunk_item = _http_chunk_list.find(key);
		if (chunk_item != _http_chunk_list.end())
		{
			// Find stream info
			std::shared_ptr<pub::Stream> stream_info;
			for (auto observer : _observers)
			{
				auto segment_publisher = std::dynamic_pointer_cast<SegmentPublisher>(observer);
				if (segment_publisher != nullptr)
				{
					stream_info = segment_publisher->GetStreamAs<pub::Stream>(request_info.vhost_app_name, request_info.stream_name);
					if (stream_info != nullptr)
					{
						// For statistics
						auto segment_request_info = SegmentRequestInfo(
							GetPublisherType(),
							*std::static_pointer_cast<info::Stream>(stream_info),
							client->GetRequest()->GetRemote()->GetRemoteAddress()->GetIpAddress(),
							static_cast<int>(chunk_item->second->sequence_number),
							is_video ? SegmentDataType::Video : SegmentDataType::Audio,
							static_cast<int64_t>(chunk_item->second->duration_in_msec / 1000));

						segment_publisher->UpdateSegmentRequestInfo(segment_request_info);

						break;
					}
				}
			}

			if (stream_info == nullptr)
			{
				// The stream has been deleted, but if it remains in the Worker queue, this code will run.
				response->SetStatusCode(http::StatusCode::NotFound);
				_http_chunk_list.clear();

				return false;
			}

			client->SetExtra(stream_info);

			// The file is being created
			logtd("Requested file is being created");

			// Set HTTP header
			response->SetHeader("Content-Type", is_video ? "video/mp4" : "audio/mp4");

			// Enable chunked transfer
			response->SetChunkedTransfer();

			// Append data to HTTP response
			response->AppendData(chunk_item->second->chunked_data);
			auto sent_bytes = response->Response();

			auto stream = GetStream(client);
			if (stream != nullptr)
			{
				MonitorInstance->IncreaseBytesOut(*stream_info, GetPublisherType(), sent_bytes);
			}

			chunk_item->second->client_list.push_back(client);

			return true;
		}
	}

	return DashStreamServer::ProcessSegmentRequest(client, request_info, segment_type);
}

void CmafStreamServer::OnCmafChunkDataPush(const ov::String &app_name, const ov::String &stream_name,
										   const ov::String &file_name,
										   const uint32_t sequence_number,
										   const uint64_t duration_in_msec,
										   bool is_video,
										   std::shared_ptr<ov::Data> &chunk_data)
{
	auto key = ov::String::FormatString("%s/%s/%s", app_name.CStr(), stream_name.CStr(), file_name.CStr());

	std::unique_lock<std::mutex> lock(_http_chunk_guard);

	auto chunk_item = _http_chunk_list.find(key);
	if (chunk_item == _http_chunk_list.end())
	{
		// New chunk data is arrived
		logtd("[%s/%s] [%s] Create a new chunk for %s, size: %zu bytes",
			  app_name.CStr(), stream_name.CStr(), StringFromPublisherType(GetPublisherType()).CStr(),
			  file_name.CStr(), chunk_data->GetLength());

		_http_chunk_list.emplace(key, std::make_shared<CmafHttpChunkedData>(sequence_number, duration_in_msec, chunk_data));
		return;
	}

	chunk_item->second->AddChunkData(chunk_data);

	auto client_item = chunk_item->second->client_list.begin();

	while (client_item != chunk_item->second->client_list.end())
	{
		auto &client = *client_item;

		auto response = std::static_pointer_cast<http::svr::h1::Http1Response>(client->GetResponse());
		if (response == nullptr)
		{
			logte("LLDASH only supports HTTP/1.1.");
			return;
		}
		auto stream_info = GetStream(client);

		if (response->SendChunkedData(chunk_data))
		{
			if (stream_info != nullptr)
			{
				MonitorInstance->IncreaseBytesOut(*stream_info, GetPublisherType(), chunk_data->GetLength());
			}
			++client_item;
		}
		else
		{
			// Maybe disconnected
			logtd("[%s/%s] [%s] Failed to send the chunked data for %s to %s (%zu bytes)",
				  app_name.CStr(), stream_name.CStr(), StringFromPublisherType(GetPublisherType()).CStr(),
				  file_name.CStr(), response->GetRemote()->ToString().CStr(), chunk_data->GetLength());

			client_item = chunk_item->second->client_list.erase(client_item);
			response->Close();
		}
	}
}

void CmafStreamServer::OnCmafChunkedComplete(const ov::String &app_name, const ov::String &stream_name,
											 const ov::String &file_name,
											 bool is_video)
{
	std::shared_ptr<CmafHttpChunkedData> chunked_data;

	{
		auto key = ov::String::FormatString("%s/%s/%s", app_name.CStr(), stream_name.CStr(), file_name.CStr());

		std::unique_lock<std::mutex> lock(_http_chunk_guard);

		auto chunk_item = _http_chunk_list.find(key);

		if (chunk_item == _http_chunk_list.end())
		{
			logtw("[%s/%s] [%s] Could not find: %s",
				  app_name.CStr(), stream_name.CStr(), StringFromPublisherType(GetPublisherType()).CStr(),
				  file_name.CStr());
			OV_ASSERT2(false);
			return;
		}

		chunked_data = chunk_item->second;

		_http_chunk_list.erase(chunk_item);
	}

	logtd("[%s/%s] [%s] The chunk is completed: %s",
		  app_name.CStr(), stream_name.CStr(), StringFromPublisherType(GetPublisherType()).CStr(),
		  file_name.CStr());

	for (auto client : chunked_data->client_list)
	{
		auto response = std::static_pointer_cast<http::svr::h1::Http1Response>(client->GetResponse());
		if (response == nullptr)
		{
			logte("LLDASH only supports HTTP/1.1.");
			return;
		}

		if (response->SendChunkedData(nullptr) == false)
		{
			logtw("[%s/%s] [%s] Could not response the CMAF chunk %s to %s",
				  app_name.CStr(), stream_name.CStr(), StringFromPublisherType(GetPublisherType()).CStr(),
				  file_name.CStr(), response->GetRemote()->ToString().CStr());
		}
	}
}