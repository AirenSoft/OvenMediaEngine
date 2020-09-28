//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#include "cmaf_stream_server.h"
#include "../dash/dash_define.h"
#include "cmaf_packetizer.h"
#include "cmaf_private.h"

HttpConnection CmafStreamServer::ProcessSegmentRequest(const std::shared_ptr<HttpClient> &client,
													   const SegmentStreamRequestInfo &request_info,
													   SegmentType segment_type)
{
	auto response = client->GetResponse();

	auto type = DashPacketizer::GetFileType(request_info.file_name);

	bool is_video = ((type == DashFileType::VideoSegment) || (type == DashFileType::VideoInit));
	std::shared_ptr<SegmentData> segment = nullptr;

	// Check if the requested file is being created
	{
		auto key = ov::String::FormatString("%s/%s/%s", request_info.vhost_app_name.CStr(), request_info.stream_name.CStr(), request_info.file_name.CStr());

		std::unique_lock<std::mutex> lock(_http_chunk_guard);

		auto chunk_item = _http_chunk_list.find(key);
		if (chunk_item != _http_chunk_list.end())
		{
			// The file is being created
			logtd("Requested file is being created");

			// Set HTTP header
			response->SetHeader("Content-Type", is_video ? "video/mp4" : "audio/mp4");

			// Enable chunked transfer
			response->SetKeepAlive();
			response->SetChunkedTransfer();

			// Append data to HTTP response
			response->AppendData(chunk_item->second->chunked_data);
			response->Response();

			chunk_item->second->client_list.push_back(client);

			return HttpConnection::KeepAlive;
		}
	}

	return DashStreamServer::ProcessSegmentRequest(client, request_info, segment_type);
}

void CmafStreamServer::OnCmafChunkDataPush(const ov::String &app_name, const ov::String &stream_name,
										   const ov::String &file_name,
										   bool is_video,
										   std::shared_ptr<ov::Data> &chunk_data)
{
	auto key = ov::String::FormatString("%s/%s/%s", app_name.CStr(), stream_name.CStr(), file_name.CStr());

	std::unique_lock<std::mutex> lock(_http_chunk_guard);

	auto chunk_item = _http_chunk_list.find(key);
	if (chunk_item == _http_chunk_list.end())
	{
		// New chunk data is arrived
		logtd("Create a new chunk for [%s/%s, %s], size: %zu bytes", app_name.CStr(), stream_name.CStr(), file_name.CStr(), chunk_data->GetLength());
		_http_chunk_list.emplace(key, std::make_shared<CmafHttpChunkedData>(chunk_data));
		return;
	}

	chunk_item->second->AddChunkData(chunk_data);

	for (auto client : chunk_item->second->client_list)
	{
		auto response = client->GetResponse();

		if (response->SendChunkedData(chunk_data) == false)
		{
			logtd("Failed to send the chunked data for [%s/%s, %s] to %s (%zu bytes)", app_name.CStr(), stream_name.CStr(), file_name.CStr(), response->GetRemote()->ToString().CStr(), chunk_data->GetLength());
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
			logtw("Could not find a CMAF chunk [%s/%s, %s]", app_name.CStr(), stream_name.CStr(), file_name.CStr());
			OV_ASSERT2(false);
			return;
		}

		chunked_data = chunk_item->second;

		_http_chunk_list.erase(chunk_item);
	}

	logtd("The chunk is completed [%s/%s, %s]", app_name.CStr(), stream_name.CStr(), file_name.CStr());

	for (auto client : chunked_data->client_list)
	{
		auto response = client->GetResponse();

		if (response->SendChunkedData(nullptr) == false)
		{
			logtw("[%s] Could not response the CMAF chunk: [%s/%s, %s]",
				  response->GetRemote()->ToString().CStr(), app_name.CStr(), stream_name.CStr(), file_name.CStr());
		}

		response->Close();
	}
}