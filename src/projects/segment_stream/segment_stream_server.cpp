//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#include "segment_stream_server.h"
#include "segment_stream_interceptor.h"
#include <sstream>
#include <regex>

#define OV_LOG_TAG "SegmentStream"

//====================================================================================================
// Start
//====================================================================================================
bool SegmentStreamServer::Start(const ov::SocketAddress &address,
                                int thread_count,
                                int max_retry_count,
                                int send_buffer_size,
                                int recv_buffer_size,
                                const std::shared_ptr<Certificate> &certificate, const std::shared_ptr<Certificate> &chain_certificate)
{
    if (_http_server != nullptr)
    {
        OV_ASSERT(false, "Server is already running");
        return false;
    }

    if(certificate != nullptr)
    {
        auto https_server = std::make_shared<HttpsServer>();

        https_server->SetLocalCertificate(certificate);
        https_server->SetChainCertificate(chain_certificate);
        https_server->SetTlsWriteToResponse(true);
        _http_server = https_server;
    }
    else
    {
        _http_server = std::make_shared<HttpServer>();
    }

    _max_retry_count = max_retry_count;

    // Interceptor add
    // - handler register
    auto process_handler = std::bind(&SegmentStreamServer::ProcessRequest,
            this,
            std::placeholders::_1,
            std::placeholders::_2,
            std::placeholders::_3,
            std::placeholders::_4);

    auto segment_stream_interceptor = std::make_shared<SegmentStreamInterceptor>(thread_count, process_handler);

//    auto process_func = std::bind(&SegmentStreamServer::ProcessRequest,
//                                this,
//                                std::placeholders::_1,
//                                std::placeholders::_2);
//
//    segment_stream_interceptor->Register(HttpMethod::Get, "", process_func, false);

    _http_server->AddInterceptor(segment_stream_interceptor);

    if (_cross_domains.empty())
    {
        _cross_domain_xml = "<?xml version=\"1.0\"?>\r\n"\
                            "<cross-domain-policy>\r\n"\
                            "<allow-access-from domain=\"*\"/>\r\n"\
                            "<site-control permitted-cross-domain-policies=\"all\"/>\r\n"\
                            "</cross-domain-policy>";
    }
    else
    {
        _cross_domains.clear();
    }

    return _http_server->Start(address, send_buffer_size, recv_buffer_size);
}


//====================================================================================================
// Stop
//====================================================================================================
bool SegmentStreamServer::Stop()
{
    return false;
}

//====================================================================================================
// monitoring data pure virtual function
// - collections vector must be insert processed
//====================================================================================================
bool SegmentStreamServer::GetMonitoringCollectionData(std::vector<std::shared_ptr<MonitoringCollectionData>> &stream_collections)
{
    return true;
}

//====================================================================================================
// 접속 허용 app 및 Porotocol 설정
//====================================================================================================
bool SegmentStreamServer::SetAllowApp(ov::String app_name, ProtocolFlag protocol_flag)
{
    auto item = _allow_apps.find(app_name);

    if (item == _allow_apps.end())
    {
        _allow_apps[app_name] = (int) protocol_flag;
    }
    else
    {
        uint32_t allow_flags = item->second;

        allow_flags |= (uint32_t) protocol_flag;

        _allow_apps[app_name] = allow_flags;
    }

    return true;
}

//====================================================================================================
// 접속 허용 app 확인
//====================================================================================================
bool SegmentStreamServer::AllowAppCheck(ov::String &app_name, ProtocolFlag protocol_flag)
{
    auto item = _allow_apps.find(app_name);

    if (item == _allow_apps.end())
    {
        return false;
    }

    return (item->second & (uint32_t) ProtocolFlag::ALL) > 0 || (item->second & (uint32_t) protocol_flag) > 0;
}

//====================================================================================================
// AddObserver
//====================================================================================================
bool SegmentStreamServer::AddObserver(const std::shared_ptr<SegmentStreamObserver> &observer)
{
    // 기존에 등록된 observer가 있는지 확인
    for (const auto &item : _observers)
    {
        if (item == observer)
        {
            // 기존에 등록되어 있음
            logtw("%p is already observer of SegmentStreamServer", observer.get());
            return false;
        }
    }

    _observers.push_back(observer);

}

//====================================================================================================
// RemoveObserver
//====================================================================================================
bool SegmentStreamServer::RemoveObserver(const std::shared_ptr<SegmentStreamObserver> &observer)
{
    auto item = std::find_if(_observers.begin(), _observers.end(),
                             [&](std::shared_ptr<SegmentStreamObserver> const &value) -> bool {
                                 return value == observer;
                             });

    if (item == _observers.end())
    {
        // 기존에 등록되어 있지 않음
        logtw("%p is not registered observer", observer.get());
        return false;
    }

    _observers.erase(item);

    return true;
}

//====================================================================================================
// Disconnect
//====================================================================================================
bool SegmentStreamServer::Disconnect(const ov::String &app_na, const ov::String &stream_name)
{
    return true;
}


//====================================================================================================
// RequestUrlParsing
// - URL 분리
//  ex) ..../app_name/stream_name/file_name.file_ext?param=param_value
//====================================================================================================
bool SegmentStreamServer::RequestUrlParsing(const ov::String &request_url,
                                            ov::String &app_name,
                                            ov::String &stream_name,
                                            ov::String &file_name,
                                            ov::String &file_ext)
{
    ov::String request_path;
    ov::String request_param;

    // 확장자 확인
    // 파라메터 분리  directory/file.ext?param=test
    auto tokens = request_url.Split("?");
    if (tokens.size() == 0) {
        return false;
    }

    request_path = tokens[0];
    request_param = tokens.size() == 2 ? tokens[1] : "";

    // ...../app_name/stream_name/file_name.ext_name 분리
    tokens.clear();
    tokens = request_path.Split("/");

    if (tokens.size() < 3) {
        return false;
    }

    app_name = tokens[tokens.size() - 3];
    stream_name = tokens[tokens.size() - 2];
    file_name = tokens[tokens.size() - 1];

    // file_name.ext_name 분리
    tokens.clear();
    tokens = file_name.Split(".");

    if (tokens.size() != 2) {
        return false;
    }

    file_ext = tokens[1];

    /*
    logtd("request : %s\n"\
         "request path : %s\n"\
         "request param : %s\n"\
         "app name : %s\n"\
         "stream name : %s\n"\
         "file name : %s\n"\
         "file ext : %s\n",
          request_url.CStr(), request_path.CStr(), request_param.CStr(),  app_name.CStr(), stream_name.CStr(), file_name.CStr(),file_ext.CStr());
    */

    return true;
}

//====================================================================================================
// ProcessRequest
// - thread call processing
//====================================================================================================
bool SegmentStreamServer::ProcessRequest(const std::shared_ptr<HttpRequest> &request,
                                         const std::shared_ptr<HttpResponse> &response,
                                         int retry_count,
                                         bool &is_retry)
{
    // prcess
    if(retry_count == 0)
        ProcessRequestUrl(request, response, request->GetRequestTarget());

    // response
    ssize_t sent = response->Response(is_retry);

    if(is_retry)
    {
        logtd("Segment response result is retry - url(%s) retry(%d/max:%d) send(%u)",
              request->GetRequestTarget().CStr(),
              retry_count,
              _max_retry_count,
              sent);
    }

    // disconnect
    if(!is_retry || retry_count >= _max_retry_count)
    {
        _http_server->Disconnect(request->GetRemote());
        is_retry = false;
    }

    return true;
}

//====================================================================================================
// ProcessRequest URL
//====================================================================================================
void SegmentStreamServer::ProcessRequestUrl(const std::shared_ptr<HttpRequest> &request,
                          const std::shared_ptr<HttpResponse> &response,
                          const ov::String &request_url)
{
    ov::String stream_name;
    ov::String app_name;
    ov::String file_name;
    ov::String file_ext;

    // Crossdomain 확인
    if (request_url.IndexOf("crossdomain.xml") >= 0)
    {
        CrossdomainRequest(request, response);
        return;
    }

    // URL parsing
    // app/strem/file.ext 기준
    if (!RequestUrlParsing(request_url, app_name, stream_name, file_name, file_ext))
    {
        logtd("Request URL Parsing Fail : %s", request_url.CStr());
        response->SetStatusCode(HttpStatusCode::NotFound);
        return;
    }

    ProtocolFlag protocol_flag = ProtocolFlag::NONE;

    if (file_ext == "m4s" || file_ext == "mpd")
        protocol_flag = ProtocolFlag::DASH;
    else if (file_ext == "ts" || file_ext == "m3u8")
        protocol_flag = ProtocolFlag::HLS;

    // CORS check
    if (request->IsHeaderExists("Origin"))
    {
        if (!CorsCheck(app_name, stream_name, file_name, protocol_flag, request, response))
        {
            response->SetStatusCode(HttpStatusCode::NotFound);// Error Response
            return;
        }
    }

    // 요청 파일 처리
    if (file_name == "playlist.m3u8")
        PlayListRequest(app_name, stream_name, file_name, protocol_flag, PlayListType::M3u8, response);
    else if (file_name == "manifest.mpd")
        PlayListRequest(app_name, stream_name, file_name, protocol_flag, PlayListType::Mpd, response);
    else if (file_ext == "ts")
        SegmentRequest(app_name, stream_name, file_name, protocol_flag, SegmentType::MpegTs, response);
    else if (file_ext == "m4s")
        SegmentRequest(app_name, stream_name, file_name, protocol_flag, SegmentType::M4S, response);
    else
        response->SetStatusCode(HttpStatusCode::NotFound);// Error Response
}

//====================================================================================================
// CorsCheck
// - Origin URL 설정 없으면 성공 Pass
//====================================================================================================
bool SegmentStreamServer::CorsCheck(ov::String &app_name,
                                    ov::String &stream_name,
                                    ov::String &file_name,
                                    ProtocolFlag protocol_flag,
                                    const std::shared_ptr<HttpRequest> &request,
                                    const std::shared_ptr<HttpResponse> &response)
{
    ov::String origin_url = request->GetHeader("ORIGIN");

    //logtd("Cors Check : %s/%s/%s - %s", app_name.CStr(), stream_name.CStr(), file_name.CStr(), origin_url.CStr());

    // cors url 설정이 있는 경우에만 확인
    if (!_cors_urls.empty())
    {
        bool find = false;
        auto item = _cors_urls.find(origin_url);

        if (item != _cors_urls.end())
        {
            if (((int) protocol_flag & (int) item->second) > 0)
            {
                find = true;
            }
        }

        if (!find)
        {
            logtd("Cors Check Fail : %s/%s/%s - %s", app_name.CStr(), stream_name.CStr(), file_name.CStr(),
                  origin_url.CStr());
            return false;
        }

    }

    // http 헤더 설정
    // response->SetHeader("Access-Control-Allow-Credentials", "true");
    // response->SetHeader("Access-Control-Allow-Headers", "Content-Type, *");
    response->SetHeader("Access-Control-Allow-Origin", origin_url);

    return true;
}

//====================================================================================================
// PlayListRequest
// - m3u8/mpd
//====================================================================================================
void SegmentStreamServer::PlayListRequest(ov::String &app_name,
                                          ov::String &stream_name,
                                          ov::String &file_name,
                                          ProtocolFlag protocol_flag,
                                          PlayListType play_list_type,
                                          const std::shared_ptr<HttpResponse> &response)
{
    if (!AllowAppCheck(app_name, protocol_flag))
    {
        logtd("App Allow Check Fail : %s/%s/%s", app_name.CStr(), stream_name.CStr(), file_name.CStr());
        response->SetStatusCode(HttpStatusCode::NotFound);
        return;
    }

    ov::String play_list;

    auto item = std::find_if(_observers.begin(), _observers.end(),
                             [&app_name, &stream_name, &file_name, &play_list_type, &play_list](
                                     auto &observer) -> bool {
                                 return observer->OnPlayListRequest(app_name, stream_name, file_name, play_list_type,
                                                                    play_list);
                             });

    if (item == _observers.end() || play_list.IsEmpty())
    {
        logtd("PlayList Serarch Fail : %s/%s/%s", app_name.CStr(), stream_name.CStr(), file_name.CStr());
        response->SetStatusCode(HttpStatusCode::NotFound);
        return;
    }

    // header setting
    if (play_list_type == PlayListType::M3u8) response->SetHeader("Content-Type", "application/x-mpegURL");
    else if (play_list_type == PlayListType::Mpd) response->SetHeader("Content-Type", "application/dash+xml");
    //response->SetHeader("Content-Length", ov::Converter::ToString(play_list.GetLength()).CStr());

    response->AppendString(play_list);

//    logtd("Playlist Resopnse : thread(%u) file(%s/%s/%s) size(%u)",
//            std::this_thread::get_id(),
//            app_name.CStr(),
//            stream_name.CStr(),
//            file_name.CStr(),
//          	play_list.GetLength());
}

//====================================================================================================
// SegmentRequest
// - ts/mp4
//====================================================================================================
void SegmentStreamServer::SegmentRequest(ov::String &app_name,
                                         ov::String &stream_name,
                                         ov::String &file_name,
                                         ProtocolFlag protocol_flag,
                                         SegmentType segment_type,
                                         const std::shared_ptr<HttpResponse> &response)
{
    if (!AllowAppCheck(app_name, protocol_flag))
    {
        logtd("App Allow Check Fail : %s/%s/%s", app_name.CStr(), stream_name.CStr(), file_name.CStr());
        response->SetStatusCode(HttpStatusCode::NotFound);
        return;
    }

    std::shared_ptr<ov::Data> segment_data = nullptr;

    auto item = std::find_if(_observers.begin(), _observers.end(),
                             [&app_name, &stream_name, &segment_type, &file_name, &segment_data](
                                     auto &observer) -> bool {
                                 return observer->OnSegmentRequest(app_name, stream_name, segment_type, file_name,
                                                                   segment_data);
                             });

    if (item == _observers.end() || segment_data == nullptr)
    {
        logtd("Segment Data Serarch Fail : %s/%s/%s", app_name.CStr(), stream_name.CStr(), file_name.CStr());
        response->SetStatusCode(HttpStatusCode::NotFound);
        return;
    }

    // header setting
    if (segment_type == SegmentType::MpegTs) response->SetHeader("Content-Type", "video/MP2T");
    else if (segment_type == SegmentType::M4S) response->SetHeader("Content-Type", "video/mp4");
    //response->SetHeader("Content-Length", ov::Converter::ToString(segment_data->GetLength()).CStr());

    response->AppendData(segment_data);

//    logtd("SegmentData Resopnse : thread(%u) file(%s/%s/%s) size(%u)",
//            std::this_thread::get_id(),
//            app_name.CStr(),
//            stream_name.CStr(),
//            file_name.CStr(),
//          	segment_data->GetLength());

}

//====================================================================================================
// CrossdomainRequest
// - corssdomain.xml
//====================================================================================================
void SegmentStreamServer::CrossdomainRequest(const std::shared_ptr<HttpRequest> &request,
                                             const std::shared_ptr<HttpResponse> &response)
{
    // header setting
    response->SetHeader("Content-Type", "text/x-cross-domain-policy");
    //response->SetHeader("Content-Length", ov::Converter::ToString(_cross_domain_xml.GetLength()).CStr());

    // data setting
    response->AppendString(_cross_domain_xml);
}

//====================================================================================================
// AddCors
// - 요청 파일 별 확장자/이름으로 프로토콜 구분 가능
// - flag 설정 필요
// - 설정 데이터가 없으면 전체 접속 허용
//====================================================================================================
void SegmentStreamServer::AddCors(const std::vector<cfg::Url> &cors_urls, ProtocolFlag protocol_flag)
{
    if (cors_urls.empty())
    {
        return;
    }

    for (auto &url : cors_urls)
    {
        auto item = _cors_urls.find(url.GetUrl());

        if (item != _cors_urls.end())
        {
            item->second |= (int) protocol_flag;
        }
        else
        {
            if(!url.GetUrl().IsEmpty())
                _cors_urls[url.GetUrl()] = (int) protocol_flag;
        }
    }
}

//====================================================================================================
// AddCrossDomain
// - crossdomain.xml 파일 요청에 대해서 전체 응답
// - 설정 데이터가 없으면 전체 접속 허용
//====================================================================================================
void SegmentStreamServer::AddCrossDomain(const std::vector<cfg::Url> &cross_domains)
{
    if (cross_domains.empty())
    {
        return;
    }

    for (auto &url : cross_domains)
    {
        if(!url.GetUrl().IsEmpty())
            _cross_domains[url.GetUrl()] = (int) ProtocolFlag::ALL;
    }

    if(_cross_domains.empty())
    {
        return;
    }

    // crossdomain.xml 재설정
    std::ostringstream cross_domain_xml;

    cross_domain_xml << "<?xml version=\"1.0\"?>\r\n";
    cross_domain_xml << "<cross-domain-policy>\r\n";
    for (auto &url : cross_domains)
    {
        cross_domain_xml << "    <allow-access-from domain=\"" << url.GetUrl().CStr() << "\"/>\r\n";
    }
    cross_domain_xml << "</cross-domain-policy>";

    _cross_domain_xml = cross_domain_xml.str().c_str();
}
