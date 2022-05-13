//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2022 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/publisher/session.h>
#include <list>

#define MAX_PENDING_REQUESTS 10

class LLHlsSession : public pub::Session
{
public:
	static std::shared_ptr<LLHlsSession> Create(session_id_t session_id,
												const std::shared_ptr<pub::Application> &application,
												const std::shared_ptr<pub::Stream> &stream);

	LLHlsSession(const info::Session &session_info, 
				const std::shared_ptr<pub::Application> &application, 
				const std::shared_ptr<pub::Stream> &stream);

	bool Start() override;
	bool Stop() override;

	// pub::Session Interface
	void SendOutgoingData(const std::any &packet) override;
	void OnMessageReceived(const std::any &message) override;

private:

	enum class RequestType : uint8_t
	{
		Playlist,
		Chunklist,
		InitializationSegment,
		Segment,
		PartialSegment,
	};

	bool ParseFileName(const ov::String &file_name, RequestType &type, int32_t &track_id, int64_t &segment_number, int64_t &partial_number);

	void ResponsePlaylist(const std::shared_ptr<http::svr::HttpExchange> &exchange);
	void ResponseChunklist(const std::shared_ptr<http::svr::HttpExchange> &exchange, const int32_t &track_id, int64_t msn, int64_t part, bool skip);
	void ResponseInitializationSegment(const std::shared_ptr<http::svr::HttpExchange> &exchange, const int32_t &track_id);
	void ResponseSegment(const std::shared_ptr<http::svr::HttpExchange> &exchange, const int32_t &track_id, const int64_t &segment_number);
	void ResponsePartialSegment(const std::shared_ptr<http::svr::HttpExchange> &exchange, const int32_t &track_id, const int64_t &segment_number, const int64_t &partial_number);

	void ResponseData(const std::shared_ptr<http::svr::HttpResponse> &response);

	void OnPlaylistUpdated(const int32_t &track_id, const int64_t &msn, const int64_t &part);

	// Pending requests
	struct PendingRequest
	{
		RequestType type;
		int32_t track_id;
		int64_t segment_number = -1;
		int64_t partial_number = -1;
		bool skip = false;

		std::shared_ptr<http::svr::HttpExchange> exchange;
	};

	bool AddPendingRequest(const std::shared_ptr<http::svr::HttpExchange> &exchange, const RequestType &type, const int32_t &track_id, const int64_t &segment_number, const int64_t &partial_number, const bool &skip = false);

	// Session runs on a single thread, so it doesn't need mutex
	std::list<PendingRequest> _pending_requests;
};