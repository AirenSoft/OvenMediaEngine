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
#include "llhls_stream.h"
#include "llhls_private.h"

std::shared_ptr<LLHlsSession> LLHlsSession::Create(session_id_t session_id,
												const std::shared_ptr<pub::Application> &application,
												const std::shared_ptr<pub::Stream> &stream)
{
	auto session_info = info::Session(*std::static_pointer_cast<info::Stream>(stream), session_id);
	auto session = std::make_shared<LLHlsSession>(session_info, application, stream);

	if (session->Start() == false)
	{
		return nullptr;
	}

	return session;		
}

LLHlsSession::LLHlsSession(const info::Session &session_info, 
							const std::shared_ptr<pub::Application> &application, 
							const std::shared_ptr<pub::Stream> &stream)
	: pub::Session(session_info, application, stream)

{

}

bool LLHlsSession::Start()
{
	return Session::Start();
}

bool LLHlsSession::Stop()
{
	return Session::Stop();
}

// pub::Session Interface
void LLHlsSession::SendOutgoingData(const std::any &alarm)
{

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
        logtd("An incorrect type of packet was input from the stream.");
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
	
	FileType file_type;
	int32_t track_id;
	int64_t segment_number;
	int64_t partial_number;

	if (ParseFileName(file, file_type, track_id, segment_number, partial_number) == false)
	{
		return;
	}

	switch (file_type)
	{
	case FileType::Playlist:
		ResponsePlaylist();
		break;
	case FileType::Chunklist:
		ResponseChunklist();
		break;
	case FileType::InitializationSegment:
		ResponseInitializationSegment();
		break;
	case FileType::Segment:
		ResponseSegment();
		break;
	case FileType::PartialSegment:
		ResponsePartialSegment();
		break;
	default:
		break;
	}
}

bool LLHlsSession::ParseFileName(const ov::String &file_name, FileType &type, int32_t &track_id, int64_t &segment_number, int64_t &partial_number)
{
	// Split the querystring if it has a '?'
	auto name_query_items = file_name.Split("?");
	
	// Split to filename.ext
	auto name_ext_items = name_query_items[0].Split(".");
	if (name_ext_items.size() != 2 || name_ext_items[1] != "m4s" || name_ext_items[1] != "m3u8")
	{
		logtw("Invalid file name requested: %s", file_name.CStr());
		return false;
	}

	// Split to <file type>_<track id>_<segment number>_<partial number>
	auto name_items = name_ext_items[0].Split("_");
	if (name_items[0] == "llhls")
	{
		if (name_ext_items[1] != "m3u8")
		{
			logtw("Invalid file name requested: %s", file_name.CStr());
			return false;
		}

		type = FileType::Playlist;
	}
	else if (name_items[0] == "chunklist")
	{
		// chunklist_<track id>_<media type>_llhls.m3u8
		if (name_items.size() != 4 || name_ext_items[1] != "m3u8")
		{
			logtw("Invalid chunklist file name requested: %s", file_name.CStr());
			return false;
		}

		type = FileType::Chunklist;
		track_id = ov::Converter::ToInt32(name_items[1].CStr());
	}
	else if (name_items[0] == "init")
	{
		// init_<track id>_<media type>
		if (name_items.size() != 3 || name_ext_items[1] != "m4s")
		{
			logtw("Invalid file name requested: %s", file_name.CStr());
			return false;
		}

		type = FileType::InitializationSegment;
		track_id = ov::Converter::ToInt32(name_items[1].CStr());
	}
	else if (name_items[0] == "seg" || name_ext_items[1] != "m4s")
	{
		// seg_<track id>_<segment number>_<media type>
		if (name_items.size() != 4)
		{
			logtw("Invalid file name requested: %s", file_name.CStr());
			return false;
		}

		type = FileType::Segment;
		track_id = ov::Converter::ToInt32(name_items[1].CStr());
		segment_number = ov::Converter::ToInt64(name_items[2].CStr());
	}
	else if (name_items[0] == "part" || name_ext_items[1] != "m4s")
	{
		// part_<track id>_<segment number>_<partial number>_<media type>
		if (name_items.size() != 5)
		{
			logtw("Invalid file name requested: %s", file_name.CStr());
			return false;
		}

		type = FileType::PartialSegment;
		track_id = ov::Converter::ToInt32(name_items[1].CStr());
		segment_number = ov::Converter::ToInt64(name_items[2].CStr());
		partial_number = ov::Converter::ToInt64(name_items[3].CStr());
	}
	else
	{
		logtw("Invalid file name requested: %s", file_name.CStr());
		return false;
	}

	return true;
}

void LLHlsSession::ResponsePlaylist()
{
	auto llhls_stream = std::static_pointer_cast<LLHlsStream>(GetStream());
	if (llhls_stream == nullptr)
	{
		return;
	}

	
}

void LLHlsSession::ResponseChunklist()
{

}

void LLHlsSession::ResponseInitializationSegment()
{

}

void LLHlsSession::ResponseSegment()
{

}

void LLHlsSession::ResponsePartialSegment()
{

}