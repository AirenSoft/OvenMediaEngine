//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "../dash/dash_stream_server.h"
#include "cmaf_interceptor.h"
#include "cmaf_packetizer.h"

class CmafStreamServer : public DashStreamServer, public ChunkedTransferInterface
{
public:
	PublisherType GetPublisherType() const noexcept override
	{
		return PublisherType::LlDash;
	}

	const char *GetPublisherName() const noexcept override
	{
		return "LLDASH Publisher";
	}

protected:
	std::shared_ptr<SegmentStreamInterceptor> CreateInterceptor() override;

	struct CmafHttpChunkedData
	{
	public:
		CmafHttpChunkedData(const uint32_t sequence_number, const uint64_t duration_in_msec, const std::shared_ptr<const ov::Data> &data)
			: sequence_number(sequence_number),
			  duration_in_msec(duration_in_msec)
		{
			chunked_data = std::make_shared<ov::Data>(104 * 1024);
			chunked_data->Append(data);
		}

		void AddChunkData(const std::shared_ptr<const ov::Data> &data)
		{
			chunked_data->Append(data);
		}

		uint32_t sequence_number = 0U;
		uint64_t duration_in_msec = 0U;
		std::shared_ptr<ov::Data> chunked_data;
		std::vector<std::shared_ptr<http::svr::HttpExchange>> client_list;
	};

	//--------------------------------------------------------------------
	// Overriding functions of DashStreamServer
	//--------------------------------------------------------------------
	bool ProcessSegmentRequest(const std::shared_ptr<http::svr::HttpExchange> &client,
													  const SegmentStreamRequestInfo &request_info, SegmentType segment_type) override;

	//--------------------------------------------------------------------
	// Implementation of ICmafChunkedTransfer
	//--------------------------------------------------------------------
	void OnCmafChunkDataPush(const ov::String &app_name, const ov::String &stream_name,
							 const ov::String &file_name,
							 const uint32_t sequence_number,
							 const uint64_t duration_in_msec,
							 bool is_video,
							 std::shared_ptr<ov::Data> &chunk_data) override;

	void OnCmafChunkedComplete(const ov::String &app_name, const ov::String &stream_name,
							   const ov::String &file_name,
							   bool is_video) override;

	// A temporary queue for the intermediate chunks
	// Key: [app name]/[stream name]/[file name]
	std::unordered_map<ov::String, std::shared_ptr<CmafHttpChunkedData>> _http_chunk_list;
	std::mutex _http_chunk_guard;
};
