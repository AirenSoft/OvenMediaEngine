//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================

#include "cmaf_stream_server.h"
#include "cmaf_private.h"

#define CMAF_SEGMENT_EXT "m4s"
#define CMAF_PLAYLIST_EXT "mpd"


//====================================================================================================
// ProcessRequest URL
//====================================================================================================
void CmafStreamServer::ProcessRequestStream(const std::shared_ptr<HttpResponse> &response,
                                            const ov::String &app_name,
                                            const ov::String &stream_name,
                                            const ov::String &file_name,
                                            const ov::String &file_ext)
{
    // request dispatch
	if (file_name == CMAF_PLAYLIST_FILE_NAME)
		PlayListRequest(app_name, stream_name, file_name, PlayListType::Mpd, response);
	else if (file_ext == CMAF_SEGMENT_EXT)
		SegmentRequest(app_name, stream_name, file_name, SegmentType::M4S, response);
	else
		response->SetStatusCode(HttpStatusCode::NotFound);// Error Response
}

//====================================================================================================
// PlayListRequest
//====================================================================================================
void CmafStreamServer::PlayListRequest(const ov::String &app_name,
										  const ov::String &stream_name,
										  const ov::String &file_name,
										  PlayListType play_list_type,
										  const std::shared_ptr<HttpResponse> &response)
{
	ov::String play_list;

	auto observer_item = std::find_if(_observers.begin(), _observers.end(),
							 [&app_name, &stream_name, &file_name, &play_list](
									 auto &observer) -> bool {
								 return observer->OnPlayListRequest(app_name, stream_name, file_name, play_list);
							 });

	if (observer_item == _observers.end() || play_list.IsEmpty())
	{
		logtd("Cmaf PlayList Serarch Fail : %s/%s/%s",
				app_name.CStr(), stream_name.CStr(), file_name.CStr());

		response->SetStatusCode(HttpStatusCode::NotFound);
		return;
	}

	// header setting
	response->SetHeader("Content-Type", "application/dash+xml");

	response->AppendString(play_list);
}

//====================================================================================================
// SegmentRequest
//====================================================================================================
void CmafStreamServer::SegmentRequest(const ov::String &app_name,
										 const ov::String &stream_name,
										 const ov::String &file_name,
										 SegmentType segment_type,
										 const std::shared_ptr<HttpResponse> &response)
{

	auto type = CmafPacketyzer::GetFileType(file_name);
	bool is_video = (type == CmafFileType::VideoSegment || type == CmafFileType::VideoInit);

	// creating chunked data response
	if(type == CmafFileType::VideoSegment || type == CmafFileType::AudioSegment)
	{
		std::map<ov::String, std::shared_ptr<CmafHttpChunkedData>> *chunked_list;
		std::mutex *chunked_guard;

	 	if(is_video)
		{
			chunked_list = &_http_chunked_video_list;
			chunked_guard = &_htttp_chunked_video_guard;
		}
		else
		{
			chunked_list = &_http_chunked_audio_list;
			chunked_guard = &_htttp_chunked_audio_guard;
		}
		// packetyzing segment check
		//  - response(client) register
		auto key = ov::String::FormatString("%s/%s/%s",  app_name.CStr(),  stream_name.CStr(), file_name.CStr());

		std::unique_lock<std::mutex> lock(*chunked_guard);

		auto chunk_item = (*chunked_list).find(key);

		if(chunk_item != (*chunked_list).end())
		{
			// header setting
			if (is_video)
				response->SetHeader("Content-Type", "video/mp4");
			else
				response->SetHeader("Content-Type", "audio/mp4");

			// send chunked transfer
			response->SetChunkedTransfer();
			response->AppendData(chunk_item->second->chunked_data);
			response->PostResponse();

			// add http response
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
		logtd("Cmaf Segment Data Serarch Fail : %s/%s/%s",
				app_name.CStr(), stream_name.CStr(), file_name.CStr());

		response->SetStatusCode(HttpStatusCode::NotFound);
		return;
	}

	// header setting
	if (is_video)
		response->SetHeader("Content-Type", "video/mp4");
	else
		response->SetHeader("Content-Type", "audio/mp4");

	// chunked transfer
	// - chunked http data( \r\n size \r\n data \r\n.....\r\n0\r\n\r\n
	response->SetChunkedTransfer();
	response->AppendData(segment->data);
}

//====================================================================================================
// OnCmafChunkDataPush
// - implement ICmafChunkedTransfer
// - create/add chunk data
// - send data
//====================================================================================================
void CmafStreamServer::OnCmafChunkDataPush(const ov::String &app_name,
										 const ov::String &stream_name,
										 const ov::String &file_name,
										 bool is_video,
										 std::shared_ptr<ov::Data> &chunk_data)
{
	std::map<ov::String, std::shared_ptr<CmafHttpChunkedData>> *chunked_list;
	std::mutex *chunked_guard;

	if(is_video)
	{
		chunked_list = &_http_chunked_video_list;
		chunked_guard = &_htttp_chunked_video_guard;
	}
	else
	{
		chunked_list = &_http_chunked_audio_list;
		chunked_guard = &_htttp_chunked_audio_guard;
	}

	auto key = ov::String::FormatString("%s/%s/%s",  app_name.CStr(),  stream_name.CStr(), file_name.CStr());

	std::unique_lock<std::mutex> lock(*chunked_guard);

	auto chunk_item = (*chunked_list).find(key);

	// create
	if(chunk_item == (*chunked_list).end())
	{
		//logtd("Cmaf chunk data create : %s/%s/%s - size(%d)",
		//		app_name.CStr(), stream_name.CStr(), file_name.CStr(), chunk_data->GetLength());

		auto http_chunked_data = std::make_shared<CmafHttpChunkedData>();
		http_chunked_data->AddChunkData(chunk_data);

		(*chunked_list)[key] = http_chunked_data;
		return;
	}

	chunk_item->second->AddChunkData(chunk_data);

	// send chunk data
	for(auto response : chunk_item->second->response_list)
	{
		if(!response->PostChunkedDataResponse(chunk_data))
		{
			//logtw("[%s] Cmaf chunked data response fail : %s/%s/%s",
			//	  response->GetRemote()->ToString().CStr(), app_name.CStr(), stream_name.CStr(), file_name.CStr());
		}
	}
}

//====================================================================================================
// OnCmafChunkedComplete
// - implement ICmafChunkedTransfer
// - remove segment
//====================================================================================================
void CmafStreamServer::OnCmafChunkedComplete(const ov::String &app_name,
										   const ov::String &stream_name,
										   const ov::String &file_name,
										   bool is_video)
{
	std::map<ov::String, std::shared_ptr<CmafHttpChunkedData>> *chunked_list;
	std::mutex *chunked_guard;

	if(is_video)
	{
		chunked_list = &_http_chunked_video_list;
		chunked_guard = &_htttp_chunked_video_guard;
	}
	else
	{
		chunked_list = &_http_chunked_audio_list;
		chunked_guard = &_htttp_chunked_audio_guard;
	}

	auto key = ov::String::FormatString("%s/%s/%s",  app_name.CStr(),  stream_name.CStr(), file_name.CStr());

	std::unique_lock<std::mutex> lock(*chunked_guard);

	auto chunk_item = (*chunked_list).find(key);

	if(chunk_item == _http_chunked_video_list.end())
	{
		logtw("Cmaf creating chunk not find : %s/%s/%s", app_name.CStr(), stream_name.CStr(), file_name.CStr());
		return;
	}

	// send chunk data(chunked transfer end)
	for(auto response : chunk_item->second->response_list)
	{
		//logtd("[%s] Cmaf chunked end response : %s/%s/%s",
		//	  response->GetRemote()->ToString().CStr(), app_name.CStr(), stream_name.CStr(), file_name.CStr());

		if(!response->PostChunkedEndResponse())
		{
			logtw("[%s] Cmaf chunked end response fail : %s/%s/%s",
				  response->GetRemote()->ToString().CStr(), app_name.CStr(), stream_name.CStr(), file_name.CStr());
		}
	}

	// remove competed segment
	(*chunked_list).erase(chunk_item);

}