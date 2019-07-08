//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================

#include "hls_stream_server.h"
#include "hls_private.h"

//====================================================================================================
// ProcessRequest URL
//====================================================================================================
void HlsStreamServer::ProcessRequestStream(const std::shared_ptr<HttpResponse> &response,
                                            const ov::String &app_name,
                                            const ov::String &stream_name,
                                            const ov::String &file_name,
                                            const ov::String &file_ext)
{

    // file extension check
    if(file_ext != HLS_SEGMENT_EXT && file_ext != HLS_PLAYLIST_EXT)
    {
        logtd("Request file extension fail - %s", file_ext.CStr());
        response->SetStatusCode(HttpStatusCode::NotFound);
        return;
    }

    // request dispatch
    if (file_name == HLS_PLAYLIST_FILE_NAME)
        PlayListRequest(app_name, stream_name, file_name, PlayListType::M3u8, response);
    else if (file_ext == HLS_SEGMENT_EXT)
        SegmentRequest(app_name, stream_name, file_name, SegmentType::MpegTs, response);
    else
        response->SetStatusCode(HttpStatusCode::NotFound);// Error Response
}



//====================================================================================================
// PlayListRequest
//====================================================================================================
void HlsStreamServer::PlayListRequest(const ov::String &app_name,
										  const ov::String &stream_name,
										  const ov::String &file_name,
										  PlayListType play_list_type,
										  const std::shared_ptr<HttpResponse> &response)
{
	ov::String play_list;

	auto item = std::find_if(_observers.begin(), _observers.end(),
							 [&app_name, &stream_name, &file_name, &play_list](
									 auto &observer) -> bool {
								 return observer->OnPlayListRequest(app_name, stream_name, file_name, play_list);
							 });

	if (item == _observers.end() || play_list.IsEmpty())
	{
		logtd("Hls PlayList Serarch Fail : %s/%s/%s", app_name.CStr(), stream_name.CStr(), file_name.CStr());
		response->SetStatusCode(HttpStatusCode::NotFound);
		return;
	}

	// header setting
	response->SetHeader("Content-Type", "application/x-mpegURL");

	response->AppendString(play_list);
}

//====================================================================================================
// SegmentRequest
//====================================================================================================
void HlsStreamServer::SegmentRequest(const ov::String &app_name,
										 const ov::String &stream_name,
										 const ov::String &file_name,
										 SegmentType segment_type,
										 const std::shared_ptr<HttpResponse> &response)
{
	std::shared_ptr<SegmentData> segment = nullptr;

	auto item = std::find_if(_observers.begin(), _observers.end(),
							 [&app_name, &stream_name, &file_name, &segment](
									 auto &observer) -> bool {
								 return observer->OnSegmentRequest(app_name, stream_name, file_name, segment);
							 });

	if (item == _observers.end())
	{
		logtd("Hls Segment Serarch Fail : %s/%s/%s", app_name.CStr(), stream_name.CStr(), file_name.CStr());
		response->SetStatusCode(HttpStatusCode::NotFound);
		return;
	}

	// header setting
	response->SetHeader("Content-Type", "video/MP2T");

	response->AppendData(segment->data);
}
