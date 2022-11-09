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
												const bool &origin_mode,
												const ov::String &session_key, 
												const std::shared_ptr<pub::Application> &application,
												const std::shared_ptr<pub::Stream> &stream,
												uint64_t session_life_time);

	LLHlsSession(const info::Session &session_info, 
				const bool &origin_mode,
				const ov::String &session_key,
				const std::shared_ptr<pub::Application> &application, 
				const std::shared_ptr<pub::Stream> &stream,
				uint64_t session_life_time);
	
	~LLHlsSession() override;

	bool Start() override;
	bool Stop() override;

	// pub::Session Interface
	void SendOutgoingData(const std::any &packet) override;
	void OnMessageReceived(const std::any &message) override;

	void OnPlayerConnected();
	uint32_t GetPlayerCount() const;

	void UpdateLastRequest(uint32_t connection_id);
	uint64_t GetLastRequestTime(uint32_t connection_id) const;
	void OnConnectionDisconnected(uint32_t connection_id);
	bool IsNoConnection() const;

	// Get session key
	const ov::String &GetSessionKey() const;

private:

	enum class RequestType : uint8_t
	{
		Playlist,
		Chunklist,
		InitializationSegment,
		Segment,
		PartialSegment,
	};

	bool ParseFileName(const ov::String &file_name, RequestType &type, int32_t &track_id, int64_t &segment_number, int64_t &partial_number, ov::String &stream_key) const;

	void ResponsePlaylist(const std::shared_ptr<http::svr::HttpExchange> &exchange, const ov::String &file_name, bool legacy);
	void ResponseChunklist(const std::shared_ptr<http::svr::HttpExchange> &exchange, const ov::String &file_name, const int32_t &track_id, int64_t msn, int64_t part, bool skip, bool legacy);
	void ResponseInitializationSegment(const std::shared_ptr<http::svr::HttpExchange> &exchange, const ov::String &file_name, const int32_t &track_id);
	void ResponseSegment(const std::shared_ptr<http::svr::HttpExchange> &exchange, const ov::String &file_name, const int32_t &track_id, const int64_t &segment_number);
	void ResponsePartialSegment(const std::shared_ptr<http::svr::HttpExchange> &exchange, const ov::String &file_name, const int32_t &track_id, const int64_t &segment_number, const int64_t &partial_number);

	void ResponseData(const std::shared_ptr<http::svr::HttpExchange> &exchange);

	void OnPlaylistUpdated(const int32_t &track_id, const int64_t &msn, const int64_t &part);

	// Pending requests
	struct PendingRequest
	{
		RequestType type;
		ov::String file_name;
		int32_t track_id;
		int64_t segment_number = -1;
		int64_t partial_number = -1;
		bool skip = false;
		bool legacy = false;

		std::shared_ptr<http::svr::HttpExchange> exchange;
	};

	bool AddPendingRequest(const std::shared_ptr<http::svr::HttpExchange> &exchange, const RequestType &type, const ov::String &file_name, const int32_t &track_id, const int64_t &segment_number, const int64_t &partial_number, const bool &skip, const bool &legacy);

	// Session runs on a single thread, so it doesn't need mutex
	std::list<PendingRequest> _pending_requests;

	// ID list of connections requesting this session
	// Connection ID : last request time
	std::map<uint32_t, uint64_t> _last_request_time;
	
	// session life time
	uint64_t _session_life_time = 0;
	uint32_t _number_of_players = 0;

	ov::String _session_key;

	int _master_playlist_max_age = 0;
	int _chunklist_max_age = 0;
	int _chunklist_with_directives_max_age = 60;
	int _segment_max_age = -1;
	int _partial_segment_max_age = -1;

	bool _origin_mode = false;
};