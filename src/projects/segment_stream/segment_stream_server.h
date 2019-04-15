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
#include "../base/publisher/publisher.h"
#include "../http_server/http_server.h"
#include "../http_server/https_server.h"
#include "../http_server/interceptors/http_request_interceptors.h"
#include "../config/config_manager.h"

enum class ProtocolFlag {
    NONE = 0x00,
    ALL = 0x01,
    DASH = 0x02,
    HLS = 0x04,

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
    bool Start(const ov::SocketAddress &address,
              int thread_count,
              int max_retry_count,
              int send_buffer_size,
              int recv_buffer_size,
              const std::shared_ptr<Certificate> &certificate = nullptr);

    bool Stop();

    bool SetAllowApp(ov::String app_name, ProtocolFlag protocol_flag);

    bool AllowAppCheck(ov::String &app_name, ProtocolFlag protocol_flag);

    bool AddObserver(const std::shared_ptr<SegmentStreamObserver> &observer);

    bool RemoveObserver(const std::shared_ptr<SegmentStreamObserver> &observer);

    bool Disconnect(const ov::String &app_na, const ov::String &stream_name);

    void AddCors(const std::vector<cfg::Url> &cors_urls, ProtocolFlag protocol_flag);

    void AddCrossDomain(const std::vector<cfg::Url> &cross_domains);

    bool GetMonitoringCollectionData(std::vector<std::shared_ptr<MonitoringCollectionData>> &collections);

protected:
    bool RequestUrlParsing(const ov::String &request_url,
                           ov::String &app_name,
                           ov::String &stream_name,
                           ov::String &file_name,
                           ov::String &file_ext);

    bool ProcessRequest(const std::shared_ptr<HttpRequest> &request,
                        const std::shared_ptr<HttpResponse> &response,
                        int retry_count,
                        bool &is_retry);

    void ProcessRequestUrl(const std::shared_ptr<HttpRequest> &request,
                            const std::shared_ptr<HttpResponse> &response,
                            const ov::String &request_url);

    bool CorsCheck(ov::String &app_name,
                   ov::String &stream_name,
                   ov::String &file_name,
                   ProtocolFlag protocol_flag,
                   const std::shared_ptr<HttpRequest> &request,
                   const std::shared_ptr<HttpResponse> &response);

    void PlayListRequest(ov::String &app_name,
                         ov::String &stream_name,
                         ov::String &file_name,
                         ProtocolFlag protocol_flag,
                         PlayListType play_list_type,
                         const std::shared_ptr<HttpResponse> &response);

    void SegmentRequest(ov::String &app_name,
                        ov::String &stream_name,
                        ov::String &file_name,
                        ProtocolFlag protocol_flag,
                        SegmentType segment_type,
                        const std::shared_ptr<HttpResponse> &response);

    void CrossdomainRequest(const std::shared_ptr<HttpRequest> &request, const std::shared_ptr<HttpResponse> &response);

protected :
    std::shared_ptr<HttpServer> _http_server;
    std::vector<std::shared_ptr<SegmentStreamObserver>> _observers;
    std::map<ov::String, int> _allow_apps;    // key : app name  value : flag(hls/dash allow flag)
    std::map<ov::String, int> _cors_urls;       // key : url  value : flag(hls/dash allow flag)
    std::map<ov::String, int> _cross_domains;   // key : url  value : flag(hls/dash allow flag)
    ov::String _cross_domain_xml;

    int _max_retry_count = 3;
};
