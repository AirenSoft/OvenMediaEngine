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
#include "cmaf_packetyzer.h"
#include "cmaf_private.h"

#define CMAF_SEGMENT_EXT "m4s"
#define CMAF_PLAYLIST_EXT "mpd"

void CmafStreamServer::ProcessRequestStream(const std::shared_ptr<HttpResponse> &response,
											const ov::String &app_name, const ov::String &stream_name,
											const ov::String &file_name, const ov::String &file_ext)
{
	if (file_ext == DASH_PLAYLIST_EXT)
	{
		OnPlayListRequest(app_name, stream_name, file_name, PlayListType::Mpd, response);
	}
	else if (file_ext == DASH_SEGMENT_EXT)
	{
		OnSegmentRequest(app_name, stream_name, file_name, SegmentType::M4S, response);
	}
	else
	{
		// Response error
		response->SetStatusCode(HttpStatusCode::NotFound);
	}
}

void CmafStreamServer::OnPlayListRequest(const ov::String &app_name, const ov::String &stream_name,
										 const ov::String &file_name,
										 PlayListType play_list_type,
										 const std::shared_ptr<HttpResponse> &response)
{
	ov::String play_list;

	auto observer_item = std::find_if(_observers.begin(), _observers.end(),
									  [&app_name, &stream_name, &file_name, &play_list](auto &observer) -> bool {
										  return observer->OnPlayListRequest(app_name, stream_name, file_name, play_list);
									  });

	if (play_list.IsEmpty())
	{
		logtd("Could not find a CMAF playlist for [%s/%s], %s", app_name.CStr(), stream_name.CStr(), file_name.CStr());
		response->SetStatusCode(HttpStatusCode::NotFound);
		return;
	}

	// Set HTTP header
	response->SetHeader("Content-Type", "application/dash+xml");

	response->AppendString(play_list);
}

void CmafStreamServer::OnSegmentRequest(const ov::String &app_name, const ov::String &stream_name,
										const ov::String &file_name,
										SegmentType segment_type,
										const std::shared_ptr<HttpResponse> &response)
{
	auto type = DashPacketyzer::GetFileType(file_name);

	bool is_video = ((type == DashFileType::VideoSegment) || (type == DashFileType::VideoInit));

	// Create a chunked data for response
	if ((type == DashFileType::VideoSegment) || (type == DashFileType::AudioSegment))
	{
		auto &chunked_list = (is_video) ? _http_chunked_video_list : _http_chunked_audio_list;
		auto &chunked_guard = (is_video) ? _http_chunked_video_guard : _http_chunked_audio_guard;

		auto key = ov::String::FormatString("%s/%s/%s", app_name.CStr(), stream_name.CStr(), file_name.CStr());

		std::unique_lock<std::mutex> lock(chunked_guard);

		auto chunk_item = chunked_list.find(key);

		if (chunk_item != chunked_list.end())
		{
			// Set HTTP header
			response->SetHeader("Content-Type", is_video ? "video/mp4" : "audio/mp4");

			// Change to chunked transfer mode
			response->SetChunkedTransfer();

			// Append HTTP response
			response->AppendData(chunk_item->second->chunked_data);
			response->PostResponse();

			chunk_item->second->response_list.push_back(response);
			return;
		}
	}

	std::shared_ptr<SegmentData> segment = nullptr;
	auto observer_item = std::find_if(_observers.begin(), _observers.end(),
									  [&app_name, &stream_name, &file_name, &segment](
										  auto &observer) -> bool {
										  return observer->OnSegmentRequest(app_name, stream_name, file_name, segment);
									  });

	if (observer_item == _observers.end())
	{
		logtd("Could not find a CMAF segment for [%s/%s], %s", app_name.CStr(), stream_name.CStr(), file_name.CStr());
		response->SetStatusCode(HttpStatusCode::NotFound);
		return;
	}

	// Set HTTP header
	response->SetHeader("Content-Type", is_video ? "video/mp4" : "audio/mp4");

	// chunked transfer
	// - chunked http data( \r\n size \r\n data \r\n.....\r\n0\r\n\r\n

	// Change to chunked transfer mode
	response->SetChunkedTransfer();
	response->AppendData(segment->data);
}

void CmafStreamServer::OnCmafChunkDataPush(const ov::String &app_name, const ov::String &stream_name,
										   const ov::String &file_name,
										   bool is_video,
										   std::shared_ptr<ov::Data> &chunk_data)
{
	auto &chunked_list = (is_video) ? _http_chunked_video_list : _http_chunked_audio_list;
	auto &chunked_guard = (is_video) ? _http_chunked_video_guard : _http_chunked_audio_guard;

	auto key = ov::String::FormatString("%s/%s/%s", app_name.CStr(), stream_name.CStr(), file_name.CStr());

	std::unique_lock<std::mutex> lock(chunked_guard);

	auto chunk_item = chunked_list.find(key);

	// create
	if (chunk_item == chunked_list.end())
	{
		//logtd("Cmaf chunk data create : %s/%s/%s - size(%d)",
		//		app_name.CStr(), stream_name.CStr(), file_name.CStr(), chunk_data->GetLength());

		auto http_chunked_data = std::make_shared<CmafHttpChunkedData>();
		http_chunked_data->AddChunkData(chunk_data);

		chunked_list[key] = http_chunked_data;
		return;
	}

	chunk_item->second->AddChunkData(chunk_data);

	// send chunk data
	for (auto response : chunk_item->second->response_list)
	{
		if (response->PostChunkedDataResponse(chunk_data) == false)
		{
			//logtw("[%s] Cmaf chunked data response fail : %s/%s/%s",
			//	  response->GetRemote()->ToString().CStr(), app_name.CStr(), stream_name.CStr(), file_name.CStr());
		}
	}
}

void CmafStreamServer::OnCmafChunkedComplete(const ov::String &app_name, const ov::String &stream_name,
											 const ov::String &file_name,
											 bool is_video)
{
	auto &chunked_list = (is_video) ? _http_chunked_video_list : _http_chunked_audio_list;
	auto &chunked_guard = (is_video) ? _http_chunked_video_guard : _http_chunked_audio_guard;

	auto key = ov::String::FormatString("%s/%s/%s", app_name.CStr(), stream_name.CStr(), file_name.CStr());

	std::unique_lock<std::mutex> lock(chunked_guard);

	auto chunk_item = chunked_list.find(key);

	if (chunk_item == _http_chunked_video_list.end())
	{
		logtw("Could not find a CMAF chunk: %s/%s, %s", app_name.CStr(), stream_name.CStr(), file_name.CStr());
		return;
	}

	// The chunk is transfered

	// send chunk data(chunked transfer end)
	for (auto response : chunk_item->second->response_list)
	{
		//logtd("[%s] Cmaf chunked end response : %s/%s/%s",
		//	  response->GetRemote()->ToString().CStr(), app_name.CStr(), stream_name.CStr(), file_name.CStr());

		if (response->PostChunkedEndResponse() == false)
		{
			logtw("[%s] Could not response the CMAF chunk: [%s/%s], %s",
				  response->GetRemote()->ToString().CStr(), app_name.CStr(), stream_name.CStr(), file_name.CStr());
		}
	}

	// remove competed segment
	chunked_list.erase(chunk_item);
}