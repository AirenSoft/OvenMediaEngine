//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include "segment_stream_observer.h"
#include <memory>
#include <http_server/http_server.h>
#include <http_server/https_server.h>
#include <http_server/interceptors/http_request_interceptors.h>


enum class AllowProtocolFlag
{
	NONE	= 0x00,
	ALL 	= 0x01,
	DASH 	= 0x02,
	HLS 	= 0x04,

};

//====================================================================================================
// SegmentStreamServer
//====================================================================================================
class SegmentStreamServer
{
public :
	SegmentStreamServer() = default;
	~SegmentStreamServer() = default;

public :
	bool Start(const ov::SocketAddress &address);
	bool Stop();
	bool SetAllowApp(ov::String &app_name, AllowProtocolFlag allow_flag);
	bool AllowAppCheck(ov::String &app_name, AllowProtocolFlag allow_flag);

	bool AddObserver(const std::shared_ptr<SegmentStreamObserver> &observer);
	bool RemoveObserver(const std::shared_ptr<SegmentStreamObserver> &observer);

	bool Disconnect(const ov::String &app_na, const ov::String &stream_name);

protected:
	bool RequestUrlParsing(const ov::String &request_url, ov::String &app_name, ov::String &stream_name, ov::String &file_name, ov::String &file_ext);
	void ProcessRequest(const std::shared_ptr<HttpRequest> &request, const std::shared_ptr<HttpResponse> &response);
	bool CorsCheck(ov::String &app_name, ov::String &stream_name, const std::shared_ptr<HttpRequest> &request, const std::shared_ptr<HttpResponse> &response);
	void PlayListRequest(ov::String &app_name, ov::String &stream_name, PlayListType play_list_type, const std::shared_ptr<HttpResponse> &response);
	void SegmentRequest(ov::String &app_name, ov::String &stream_name, ov::String &file_name,SegmentType segment_type, const std::shared_ptr<HttpResponse> &response);
	void CrossdomainRequest(const std::shared_ptr<HttpRequest> &request, const std::shared_ptr<HttpResponse> &response);


protected :
	std::shared_ptr<HttpServer> 						_http_server;
	std::vector<std::shared_ptr<SegmentStreamObserver>> _observers;
	std::map<ov::String, uint32_t> 			        _allow_apps;  // key : app name  flag : hls/dash allow flag


};
