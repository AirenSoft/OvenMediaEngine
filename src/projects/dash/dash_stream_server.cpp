//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================

#include "dash_stream_server.h"
#include "dash_private.h"

#define SEGMENT_EXT "m4s"
#define PLAYLIST_EXT "mpd"
#define PLAYLIST_FILE_NAME "manifest.mpd"

//====================================================================================================
// ProcessRequest URL
//====================================================================================================
void DashStreamServer::ProcessRequestStream(const std::shared_ptr<HttpRequest> &request,
                                            const std::shared_ptr<HttpResponse> &response,
                                            const ov::String &app_name,
                                            const ov::String &stream_name,
                                            const ov::String &file_name,
                                            const ov::String &file_ext)
{
    // file extension check
    if(file_ext != SEGMENT_EXT && file_ext != PLAYLIST_EXT)
    {
        logtd("Request file extension fail - %s", file_ext.CStr());
        response->SetStatusCode(HttpStatusCode::NotFound);
        return;
    }

    // request dispatch
   if (file_name == PLAYLIST_FILE_NAME)
        PlayListRequest(app_name, stream_name, file_name, PlayListType::Mpd, response);
    else if (file_ext == SEGMENT_EXT)
        SegmentRequest(app_name, stream_name, file_name, SegmentType::M4S, response);
    else
        response->SetStatusCode(HttpStatusCode::NotFound);// Error Response
}