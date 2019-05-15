//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================

#include "monitoring_server.h"
#include "monitoring_interceptor.h"
#include <sstream>
#include <iomanip>

#define OV_LOG_TAG "Monitoring"

ov::String GetCurrentIso8601Time()
{
    time_t now = time(nullptr);
#if 0
    std::tm tm = *std::localtime(&now);

    std::ostringstream string_stream;
    string_stream << std::put_time(&tm, "%FT%T");
#else
    std::tm tm = *std::gmtime(&now);

	std::ostringstream string_stream;
	string_stream << std::put_time(&tm, "%FT%TZ");
#endif
    ov::String result = string_stream.str().c_str();

    return result;
}

//====================================================================================================
// Start
//====================================================================================================
bool MonitoringServer::Start(const ov::SocketAddress &address,
                             const std::vector<std::shared_ptr<pvd::Provider>> &providers,
                             const std::vector<std::shared_ptr<Publisher>> &publishers,
                             const std::shared_ptr<Certificate> &certificate)
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

        _http_server = https_server;
    }
    else
    {
        _http_server = std::make_shared<HttpServer>();
    }

    auto monitoring_interceptor = std::make_shared<MonitoringInterceptor>();

    // app_name/stream_name/file_name.ext?param=value
    ov::String regular_expression = "(([^ #?]*)/([^ #?]*)/)?(stat)[?]?([^ #]*)#?([^ ]*)?";

    auto process_func = std::bind(&MonitoringServer::ProcessRequest,
                                    this,
                                    std::placeholders::_1,
                                    std::placeholders::_2);

    std::static_pointer_cast<HttpDefaultInterceptor>(monitoring_interceptor)->Register(HttpMethod::Get,
                                                                                    regular_expression,
                                                                                    process_func);

    _http_server->AddInterceptor(std::static_pointer_cast<HttpRequestInterceptor>(monitoring_interceptor));

    _providers.assign(providers.begin(), providers.end());
    _publishers.assign(publishers.begin(), publishers.end());

    logtd("Monitoring Server Start - provider(%u) publisher(%u)", _providers.size(), _publishers.size());

    return _http_server->Start(address);
}

//====================================================================================================
// Stop
//====================================================================================================
bool MonitoringServer::Stop()
{
    return false;
}

//====================================================================================================
// Disconnect
//====================================================================================================
bool MonitoringServer::Disconnect(const ov::String &app_na, const ov::String &stream_name)
{
    return true;
}

//====================================================================================================
// RequestUrlParsing
// - URL split
//  ex) ..../app_name/stream_name/file_name.file_ext?param=param_value
//====================================================================================================
bool MonitoringServer::RequestUrlParsing(const ov::String &request_url,
                                            ov::String &file_name,
                                            ov::String &file_ext)
{
    ov::String request_path;
    ov::String request_param;

    // param split directory/file.ext?param=test
    auto tokens = request_url.Split("?");
    if (tokens.size() == 0)
    {
        return false;
    }

    request_path = tokens[0];
    request_param = tokens.size() == 2 ? tokens[1] : "";

    // ...../app_name/stream_name/file_name.ext_name split
    tokens.clear();
    tokens = request_path.Split("/");

    if (tokens.size() < 2)
    {
        return false;
    }

    file_name = tokens[tokens.size() - 1];

    // file_name.ext_name seprator
    tokens.clear();
    tokens = file_name.Split(".");
    if (tokens.size() == 2)
    {
        file_ext = tokens[1];
    }

    logtd("request : %s\n"\
         "request path : %s\n"\
         "request param : %s\n"\
         "file name : %s\n"\
         "file ext : %s\n",
          request_url.CStr(), request_path.CStr(), request_param.CStr(),  file_name.CStr(),file_ext.CStr());

    return true;
}

//====================================================================================================
// ProcessRequest
//====================================================================================================
void MonitoringServer::ProcessRequest(const std::shared_ptr<HttpRequest> &request,
                                         const std::shared_ptr<HttpResponse> &response)
{
    ov::String request_url = request->GetRequestTarget();
    ov::String file_name;
    ov::String file_ext;

    logtd("Request URL  : %s", request_url.CStr());

    // URL parsing
    if (!RequestUrlParsing(request_url, file_name, file_ext))
    {
        logtd("Request URL Parsing Fail : %s", request_url.CStr());
        response->SetStatusCode(HttpStatusCode::NotFound);
        response->Response();
        return;
    }
    // request file process
    if (file_name == "stat")
        StateRequest(response);
    else if(file_name == "test")
        StateRequest(response);
    else
    {
        response->SetStatusCode(HttpStatusCode::NotFound);
        response->Response();
    }
}

//====================================================================================================
// StateRequest
// - app sum
// - origin sum
// - host sum
//
// {org},{app},{stream},{tot},{edge},{p2p},{bitrate_avg},{bitrate_sum_edge},{bitrate_sum_p2p},{bitrate_sum_tot},{datetime}
// ex)
//      211.222.238.223,live,stream2,100,80,20,1000000,80000000,20000000,100000000,2019-03-25T09:58:58+00:00
//      211.222.238.223,live,stream3,120,100,20,1000000,100000000,20000000,120000000,2019-03-25T09:58:58+00:00
//====================================================================================================
#define COLLECTION_DATA_SEPARATOR (',')
#define COLLECTION_DATA_LINE_END ("\n")
#include <map>
void MonitoringServer::StateRequest(const std::shared_ptr<HttpResponse> &response)
{
    std::map<std::pair<ov::String, ov::String>, std::shared_ptr<MonitoringCollectionData>> stream_sum; // key : app + stream pair
    std::map<ov::String, std::shared_ptr<MonitoringCollectionData>> app_sum; // key : app name
    std::map<ov::String, std::shared_ptr<MonitoringCollectionData>> origin_sum; // key : app name
    std::chrono::system_clock::time_point current_time = std::chrono::system_clock::now();

    std::shared_ptr<MonitoringCollectionData> host_sum =
            std::make_shared<MonitoringCollectionData>(MonitroingCollectionType::Host);// host(total)

    std::vector<std::shared_ptr<MonitoringCollectionData>> collections;

    // stream sum
    for (const auto &publisher : _publishers)
    {
        publisher->GetMonitoringCollectionData(collections);
    }

    // stream/app/origin/host sum
    for (const auto &collection : collections)
    {
        // stream collection sum
        // app collection sum
        auto stream_item = stream_sum.find(std::pair<ov::String, ov::String>(collection->app_name, collection->stream_name));
        if(stream_item == stream_sum.end())
        {
            auto stream_collection = std::make_shared<MonitoringCollectionData>(MonitroingCollectionType::Stream,
                                                                             collection->origin_name,
                                                                             collection->app_name,
                                                                             collection->stream_name);
            stream_collection->Append(collection);
            stream_collection->check_time = current_time;

            // push
            stream_sum[std::pair<ov::String, ov::String>(collection->app_name, collection->stream_name)] = stream_collection;
        }
        else
        {
            stream_item->second->Append(collection);
        }

        // app collection sum
        auto app_item = app_sum.find(collection->app_name);
        if(app_item == app_sum.end())
        {
            auto app_collection = std::make_shared<MonitoringCollectionData>(MonitroingCollectionType::App,
                                                                             collection->origin_name,
                                                                             collection->app_name);
            app_collection->Append(collection);
            app_collection->check_time = current_time;

            // push
            app_sum[app_collection->app_name] = app_collection;
        }
        else
        {
            app_item->second->Append(collection);
        }

        // origin collection sum
        auto origin_item = origin_sum.find(collection->app_name);
        if(origin_item == origin_sum.end())
        {
            auto origin_collection = std::make_shared<MonitoringCollectionData>(MonitroingCollectionType::Origin,
                                                                                collection->origin_name);
            origin_collection->Append(collection);
            origin_collection->check_time = current_time;

            // push
            origin_sum[origin_collection->origin_name] = origin_collection;
        }
        else
        {
            origin_item->second->Append(collection);
        }

        // host collection sum
        host_sum->Append(collection);
    }

    // clear
    collections.clear();

    // stream collection push
    for (const auto &item : stream_sum)
        collections.push_back(item.second);

    // app collection push
    for (const auto &item : app_sum)
        collections.push_back(item.second);

    // origin collection push
    for (const auto &item : origin_sum)
        collections.push_back(item.second);

    // host collection push
    collections.push_back(host_sum);

    std::ostringstream string_stream;

    for(const auto &collection : collections)
    {
        uint32_t total_connection = collection->edge_connection + collection->p2p_connection;

        if(total_connection == 0)
            continue;

        uint64_t total_bitrate = collection->edge_bitrate + collection->p2p_bitrate;
        uint64_t avg_bitrate = total_bitrate/total_connection;

        string_stream
        << collection->type_string.CStr()    << COLLECTION_DATA_SEPARATOR
        << collection->origin_name.CStr()    << COLLECTION_DATA_SEPARATOR
        << collection->app_name.CStr()       << COLLECTION_DATA_SEPARATOR
        << collection->stream_name.CStr()    << COLLECTION_DATA_SEPARATOR
        << total_connection                  << COLLECTION_DATA_SEPARATOR
        << collection->edge_connection       << COLLECTION_DATA_SEPARATOR
        << collection->p2p_connection        << COLLECTION_DATA_SEPARATOR
        << avg_bitrate                       << COLLECTION_DATA_SEPARATOR
        << collection->edge_bitrate          << COLLECTION_DATA_SEPARATOR
        << collection->p2p_bitrate           << COLLECTION_DATA_SEPARATOR
        << total_bitrate                     << COLLECTION_DATA_SEPARATOR
        << GetCurrentIso8601Time().CStr()    << COLLECTION_DATA_LINE_END;
    }

    ov::String data = string_stream.str().c_str();

    response->AppendString(data);

    if (!response->Response())
    {
        logte("State Response Fail");
    }
}