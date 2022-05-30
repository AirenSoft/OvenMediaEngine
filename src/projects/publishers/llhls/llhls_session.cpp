//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2022 AirenSoft. All rights reserved.
//
//==============================================================================
#include <modules/http/server/http_exchange.h>
#include "llhls_session.h"
#include "llhls_application.h"
#include "llhls_stream.h"
#include "llhls_private.h"

std::shared_ptr<LLHlsSession> LLHlsSession::Create(session_id_t session_id, const ov::String &session_key,
												const std::shared_ptr<pub::Application> &application,
												const std::shared_ptr<pub::Stream> &stream,
												uint64_t session_life_time)
{
	auto session_info = info::Session(*std::static_pointer_cast<info::Stream>(stream), session_id);
	auto session = std::make_shared<LLHlsSession>(session_info, session_key, application, stream, session_life_time);

	if (session->Start() == false)
	{
		return nullptr;
	} 

	return session;
}

LLHlsSession::LLHlsSession(const info::Session &session_info, const ov::String &session_key,
							const std::shared_ptr<pub::Application> &application, 
							const std::shared_ptr<pub::Stream> &stream,
							uint64_t session_life_time)
	: pub::Session(session_info, application, stream)

{
	_session_life_time = session_life_time;
	if (session_key.IsEmpty())
	{
		_session_key = ov::Random::GenerateString(8);
	}

	logtd("LLHlsSession::LLHlsSession (%d)", session_info.GetId());
}

LLHlsSession::~LLHlsSession()
{
	logtd("LLHlsSession::~LLHlsSession(%d)", GetId());
	MonitorInstance->OnSessionsDisconnected(*GetStream(), PublisherType::LLHls, _number_of_players);
}

bool LLHlsSession::Start()
{
	return Session::Start();
}

bool LLHlsSession::Stop()
{
	return Session::Stop();
}

const ov::String &LLHlsSession::GetSessionKey() const
{
	return _session_key;
}

void LLHlsSession::UpdateLastRequest(uint32_t connection_id)
{
	_last_request_time[connection_id] = ov::Clock::NowMSec();

	logtd("LLHlsSession(%u) : Request updated from %u : size(%d)", GetId(), connection_id, _last_request_time.size());
}

uint64_t LLHlsSession::GetLastRequestTime(uint32_t connection_id) const
{
	auto itr = _last_request_time.find(connection_id);
	if (itr == _last_request_time.end())
	{
		return 0;
	}

	return itr->second;
}

void LLHlsSession::OnConnectionDisconnected(uint32_t connection_id)
{
	_last_request_time.erase(connection_id);

	logtd("LLHlsSession(%u) : Disconnected from %u : size(%d)", GetId(), connection_id, _last_request_time.size());
}

bool LLHlsSession::IsNoConnection() const
{
	return _last_request_time.empty();
}

// pub::Session Interface
void LLHlsSession::SendOutgoingData(const std::any &notification)
{
	// Check expired time
	if(_session_life_time != 0 && _session_life_time < ov::Clock::NowMSec())
	{
		return;
	}

	std::shared_ptr<LLHlsStream::PlaylistUpdatedEvent> event;
	try
	{
		event = std::any_cast<std::shared_ptr<LLHlsStream::PlaylistUpdatedEvent>>(notification);
		if (event == nullptr)
		{
			return;
		}
	}
	catch (const std::bad_any_cast& e)
	{
		logtc("LLHlsSession : Invalid notification type : %s", e.what());
		return;
	}

	OnPlaylistUpdated(event->track_id, event->msn, event->part);
}

void LLHlsSession::OnMessageReceived(const std::any &message)
{
	std::shared_ptr<http::svr::HttpExchange> exchange = nullptr;
	try 
	{
        exchange = std::any_cast<std::shared_ptr<http::svr::HttpExchange>>(message);
		if(exchange == nullptr)
		{
			return;
		}
    }
    catch(const std::bad_any_cast& e) 
	{
        logtc("An incorrect type of packet was input from the stream.");
		return;
    }

	// Check expired time
	if(_session_life_time != 0 && _session_life_time < ov::Clock::NowMSec())
	{
		auto response = exchange->GetResponse();

		response->SetStatusCode(http::StatusCode::Unauthorized);
		response->Response();
		exchange->Release();
		return;
	}

	logtd("LLHlsSession::OnMessageReceived(%u) - %s", GetId(), exchange->ToString().CStr());

	auto request = exchange->GetRequest();
	auto request_uri = request->GetParsedUri();

	if (request_uri == nullptr)
	{
		return;
	}

	auto file = request_uri->File();
	if (file.IsEmpty())
	{
		return;
	}
	
	RequestType file_type;
	int32_t track_id;
	int64_t segment_number;
	int64_t partial_number;
	ov::String stream_key;

	if (ParseFileName(file, file_type, track_id, segment_number, partial_number, stream_key) == false)
	{
		return;
	}

	auto llhls_stream = std::static_pointer_cast<LLHlsStream>(GetStream());
	if (llhls_stream == nullptr)
	{
		return;
	}

	if (file_type != RequestType::Playlist && file_type != RequestType::Chunklist)
	{
		// All reqeusts except playlist have a stream key
		if (stream_key != llhls_stream->GetStreamKey())
		{
			logtw("LLHlsSession::OnMessageReceived(%u) - Invalid stream key : %s (expected : %s)", GetId(), stream_key.CStr(), llhls_stream->GetStreamKey().CStr());
			return;
		}
	}

	switch (file_type)
	{
	case RequestType::Playlist:
		ResponsePlaylist(exchange);
		break;
	case RequestType::Chunklist:
	{
		int64_t msn = -1, part = -1;
		bool skip = false;
		// ?_HLS_msn=<M>&_HLS_part=<N>&_HLS_skip=YES|v2
		if (request_uri->HasQueryKey("_HLS_msn"))
		{
			msn = ov::Converter::ToInt64(request_uri->GetQueryValue("_HLS_msn").CStr());
		}

		if (request_uri->HasQueryKey("_HLS_part"))
		{
			part = ov::Converter::ToInt64(request_uri->GetQueryValue("_HLS_part").CStr());
		}

		if (request_uri->HasQueryKey("_HLS_skip"))
		{
			if (request_uri->GetQueryValue("_HLS_skip") == "YES")
			{
				skip = true;
			}
			// v2 is not supported yet
		}

		ResponseChunklist(exchange, track_id, msn, part, skip);
		break;
	}
	case RequestType::InitializationSegment:
		ResponseInitializationSegment(exchange, track_id);
		break;
	case RequestType::Segment:
		ResponseSegment(exchange, track_id, segment_number);
		break;
	case RequestType::PartialSegment:
		ResponsePartialSegment(exchange, track_id, segment_number, partial_number);
		break;
	default:
		break;
	}
}

bool LLHlsSession::ParseFileName(const ov::String &file_name, RequestType &type, int32_t &track_id, int64_t &segment_number, int64_t &partial_number, ov::String &stream_key) const
{
	// Split to filename.ext
	auto name_ext_items = file_name.Split(".");
	if (name_ext_items.size() < 2 || (name_ext_items[1] != "m4s" && name_ext_items[1] != "m3u8"))
	{
		logtw("Invalid file name requested: %s", file_name.CStr());
		return false;
	}

	auto name_items = name_ext_items[0].Split("_");
	if (name_items[0] == "llhls")
	{
		// llhls.m3u8
		if (name_ext_items[1] != "m3u8")
		{
			logtw("Invalid file name requested: %s", file_name.CStr());
			return false;
		}

		type = RequestType::Playlist;
	}
	else if (name_items[0] == "chunklist")
	{
		// chunklist_<track id>_<media type>_llhls.m3u8?session_key=<key>&_HLS_msn=<M>&_HLS_part=<N>&_HLS_skip=YES|v2
		if (name_items.size() != 4 || name_ext_items[1] != "m3u8")
		{
			logtw("Invalid chunklist file name requested: %s", file_name.CStr());
			return false;
		}

		type = RequestType::Chunklist;
		track_id = ov::Converter::ToInt32(name_items[1].CStr());
	}
	else if (name_items[0] == "init")
	{
		// init_<track id>_<media type>_<stream key>_llhls
		if (name_items.size() != 5 || name_ext_items[1] != "m4s")
		{
			logtw("Invalid file name requested: %s", file_name.CStr());
			return false;
		}

		type = RequestType::InitializationSegment;
		track_id = ov::Converter::ToInt32(name_items[1].CStr());
		stream_key = name_items[3];
	}
	else if (name_items[0] == "seg" || name_ext_items[1] != "m4s")
	{
		// seg_<track id>_<segment number>_<media type>_<stream key>_llhls
		if (name_items.size() != 6)
		{
			logtw("Invalid file name requested: %s", file_name.CStr());
			return false;
		}

		type = RequestType::Segment;
		track_id = ov::Converter::ToInt32(name_items[1].CStr());
		segment_number = ov::Converter::ToInt64(name_items[2].CStr());
		stream_key = name_items[4];
	}
	else if (name_items[0] == "part" || name_ext_items[1] != "m4s")
	{
		// part_<track id>_<segment number>_<partial number>_<media type>_<stream key>_llhls
		if (name_items.size() != 7)
		{
			logtw("Invalid file name requested: %s", file_name.CStr());
			return false;
		}

		type = RequestType::PartialSegment;
		track_id = ov::Converter::ToInt32(name_items[1].CStr());
		segment_number = ov::Converter::ToInt64(name_items[2].CStr());
		partial_number = ov::Converter::ToInt64(name_items[3].CStr());
		stream_key = name_items[5];
	}
	else
	{
		logtw("Invalid file name requested: %s", file_name.CStr());
		return false;
	}

	return true;
}

void LLHlsSession::ResponsePlaylist(const std::shared_ptr<http::svr::HttpExchange> &exchange)
{
	auto llhls_stream = std::static_pointer_cast<LLHlsStream>(GetStream());
	if (llhls_stream == nullptr)
	{
		return;
	}

	auto request = exchange->GetRequest();
	auto response = exchange->GetResponse();
	auto request_uri = exchange->GetRequest()->GetParsedUri();

	ov::String content_encoding = "identity";
	bool gzip = false;
	auto encodings = request->GetHeader("Accept-Encoding");
	if (encodings.IndexOf("gzip") >= 0 || encodings.IndexOf("*") >= 0)
	{
		gzip = true;
		content_encoding = "gzip";
	}

	// Get the playlist
	auto query_string = ov::String::FormatString("session=%u_%s", GetId(), _session_key.CStr());
	auto [result, playlist] = llhls_stream->GetPlaylist(query_string, gzip);
	if (result == LLHlsStream::RequestResult::Success)
	{
		// Send the playlist
		response->SetStatusCode(http::StatusCode::OK);
		// Set Content-Type header
		response->SetHeader("Content-Type", "application/vnd.apple.mpegurl");
		// gzip compression
		response->SetHeader("Content-Encoding", content_encoding);

		// Cache-Control header
		// When the stream is recreated, llhls.m3u8 file is changed.
		auto cache_control = ov::String::FormatString("no-cache");
		response->SetHeader("Cache-Control", cache_control);

		response->AppendData(playlist);

		MonitorInstance->OnSessionConnected(*GetStream(), PublisherType::LLHls);
		_number_of_players += 1;
	}
	else if (result == LLHlsStream::RequestResult::Accepted)
	{
		// llhls.m3u8 is transmitted when more than one segment (any track) is created.
		AddPendingRequest(exchange, RequestType::Playlist, 0, 1, 0);
		return ;
	}
	else
	{
		// Send error response
		response->SetStatusCode(http::StatusCode::NotFound);
	}

	ResponseData(exchange);
}

void LLHlsSession::ResponseChunklist(const std::shared_ptr<http::svr::HttpExchange> &exchange, const int32_t &track_id, int64_t msn, int64_t part, bool skip)
{
	auto llhls_stream = std::static_pointer_cast<LLHlsStream>(GetStream());
	if (llhls_stream == nullptr)
	{
		return;
	}

	auto request = exchange->GetRequest();
	auto response = exchange->GetResponse();

	if (msn == -1 && part == -1)
	{
		// If there are not enough segments and chunks in the beginning, 
		// the player cannot start playing, so the request is pending until at least one segment is created.
		msn = 2;
		part = 0;
	}

	ov::String content_encoding = "identity";
	bool gzip = false;
	auto encodings = request->GetHeader("Accept-Encoding");
	if (encodings.IndexOf("gzip") >= 0 || encodings.IndexOf("*") >= 0)
	{
		gzip = true;
		content_encoding = "gzip";
	}

	// Get the chunklist
	auto query_string = ov::String::FormatString("session=%u_%s", GetId(), _session_key.CStr());
	auto [result, chunklist] = llhls_stream->GetChunklist(query_string, track_id, msn, part, skip, gzip);
	if (result == LLHlsStream::RequestResult::Success)
	{
		// Send the chunklist
		response->SetStatusCode(http::StatusCode::OK);
		// Set Content-Type header
		response->SetHeader("Content-Type", "application/vnd.apple.mpegurl");
		// gzip compression
		response->SetHeader("Content-Encoding", content_encoding);

		// Cache-Control header
		auto cache_control = ov::String::FormatString("no-cache");
		response->SetHeader("Cache-Control", cache_control);

		response->AppendData(chunklist);

		// If a client uses previously cached llhls.m3u8 and requests chunklist
		if (_number_of_players == 0)
		{
			MonitorInstance->OnSessionConnected(*GetStream(), PublisherType::LLHls);
			_number_of_players += 1;
		}
	}
	else if (result == LLHlsStream::RequestResult::Accepted)
	{
		// Hold
		//TODO(Getroot): EXT-X-SKIP is under debugging
		skip = false;
		AddPendingRequest(exchange, RequestType::Chunklist, track_id, msn, part, skip);
		return ;
	}
	else
	{
		// Send error response
		response->SetStatusCode(http::StatusCode::NotFound);
	}

	ResponseData(exchange);
}

void LLHlsSession::ResponseInitializationSegment(const std::shared_ptr<http::svr::HttpExchange> &exchange, const int32_t &track_id)
{
	auto llhls_stream = std::static_pointer_cast<LLHlsStream>(GetStream());
	if (llhls_stream == nullptr)
	{
		return;
	}

	auto response = exchange->GetResponse();

	// Get the initialization segment
	auto [result, initialization_segment] = llhls_stream->GetInitializationSegment(track_id);
	if (result == LLHlsStream::RequestResult::Success)
	{
		// Send the initialization segment
		response->SetStatusCode(http::StatusCode::OK);
		// Set Content-Type header
		// Set Content-Type header
		if (GetStream()->GetTrack(track_id)->GetMediaType() == cmn::MediaType::Video)
		{
			response->SetHeader("Content-Type", "video/mp4");
		}
		else
		{
			response->SetHeader("Content-Type", "audio/mp4");
		}
		response->AppendData(initialization_segment);
	}
	else
	{
		// Send error response
		response->SetStatusCode(http::StatusCode::NotFound);
	}

	ResponseData(exchange);
}

void LLHlsSession::ResponseSegment(const std::shared_ptr<http::svr::HttpExchange> &exchange, const int32_t &track_id, const int64_t &segment_number)
{
	auto llhls_stream = std::static_pointer_cast<LLHlsStream>(GetStream());
	if (llhls_stream == nullptr)
	{
		return;
	}

	auto response = exchange->GetResponse();

	// Get the segment
	auto [result, segment] = llhls_stream->GetSegment(track_id, segment_number);
	if (result == LLHlsStream::RequestResult::Success)
	{
		// Send the segment
		response->SetStatusCode(http::StatusCode::OK);
		// Set Content-Type header
		if (GetStream()->GetTrack(track_id)->GetMediaType() == cmn::MediaType::Video)
		{
			response->SetHeader("Content-Type", "video/mp4");
		}
		else
		{
			response->SetHeader("Content-Type", "audio/mp4");
		}
		response->AppendData(segment);
	}
	else
	{
		// Send error response
		response->SetStatusCode(http::StatusCode::NotFound);
	}

	ResponseData(exchange);
}

void LLHlsSession::ResponsePartialSegment(const std::shared_ptr<http::svr::HttpExchange> &exchange, const int32_t &track_id, const int64_t &segment_number, const int64_t &partial_number)
{
	auto llhls_stream = std::static_pointer_cast<LLHlsStream>(GetStream());
	if (llhls_stream == nullptr)
	{
		return;
	}

	auto response = exchange->GetResponse();

	// Get the partial segment
	auto [result, partial_segment] = llhls_stream->GetChunk(track_id, segment_number, partial_number);
	if (result == LLHlsStream::RequestResult::Success)
	{
		// Send the partial segment
		response->SetStatusCode(http::StatusCode::OK);
		// Set Content-Type header
		if (GetStream()->GetTrack(track_id)->GetMediaType() == cmn::MediaType::Video)
		{
			response->SetHeader("Content-Type", "video/mp4");
		}
		else
		{
			response->SetHeader("Content-Type", "audio/mp4");
		}
		response->AppendData(partial_segment);
	}
	else if (result == LLHlsStream::RequestResult::Accepted)
	{
		// Hold
		AddPendingRequest(exchange, RequestType::PartialSegment, track_id, segment_number, partial_number);
		return ;
	}
	else
	{
		// Send error response
		response->SetStatusCode(http::StatusCode::NotFound);
	}

	ResponseData(exchange);
}

void LLHlsSession::ResponseData(const std::shared_ptr<http::svr::HttpExchange> &exchange)
{
	auto response = exchange->GetResponse();
	auto sent_size = response->Response();
	MonitorInstance->IncreaseBytesOut(*GetStream(), PublisherType::LLHls, sent_size);

	// Terminate the HTTP/2 stream
	exchange->Release();
}

void LLHlsSession::OnPlaylistUpdated(const int32_t &track_id, const int64_t &msn, const int64_t &part)
{
	logtd("LLHlsSession::OnPlaylistUpdated track_id: %d, msn: %lld, part: %lld", track_id, msn, part);
	// Find the pending request
	auto it = _pending_requests.begin();
	while (it != _pending_requests.end())
	{
		if ( (it->type == RequestType::Playlist) &&
			 ((it->segment_number < msn) || (it->segment_number <= msn && it->partial_number <= part)) )
		{
			// Send the playlist
			auto exchange = it->exchange;
			ResponsePlaylist(exchange);
			it = _pending_requests.erase(it);
		}
		else if ( (it->track_id == track_id) && 
			 ((it->segment_number < msn) || (it->segment_number <= msn && it->partial_number <= part)) )
		{
			// Resume the request
			switch (it->type)
			{
			case RequestType::Chunklist:
				ResponseChunklist(it->exchange, it->track_id, it->segment_number, it->partial_number, it->skip);
				break;
			case RequestType::PartialSegment:
				ResponsePartialSegment(it->exchange, it->track_id, it->segment_number, it->partial_number);
				break;
			case RequestType::Segment:
				ResponseSegment(it->exchange, it->track_id, it->segment_number);
				break;
			case RequestType::Playlist:
			case RequestType::InitializationSegment:
			default:
				// Assertion
				OV_ASSERT2(false);
				break;
			}

			// Remove the request
			it = _pending_requests.erase(it);
		}
		else
		{
			// Next
			++it;
		}
	}
}

bool LLHlsSession::AddPendingRequest(const std::shared_ptr<http::svr::HttpExchange> &exchange, const RequestType &type, const int32_t &track_id, const int64_t &segment_number, const int64_t &partial_number, const bool &skip)
{
	// Add the request to the pending list
	PendingRequest request;
	request.type = type;
	request.track_id = track_id;
	request.segment_number = segment_number;
	request.partial_number = partial_number;
	request.skip = skip;
	request.exchange = exchange;

	// Add the request to the pending list
	_pending_requests.push_back(request);

	if (_pending_requests.size() > MAX_PENDING_REQUESTS)
	{
		logtd("[%s/%s/%u] Too many pending requests (%u)", 
				GetApplication()->GetName().CStr(),
				GetStream()->GetName().CStr(),
				GetId(),
				_pending_requests.size());
	}

	return true;
}