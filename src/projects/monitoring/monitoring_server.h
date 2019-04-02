//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include <memory>
#include "../http_server/http_server.h"
#include "../http_server/https_server.h"
#include "../http_server/interceptors/http_request_interceptors.h"
#include "../config/config_manager.h"
#include "../base/ovsocket/ovsocket.h"
#include "../base/ovcrypto/certificate.h"
#include "../base/provider/provider.h"
#include "../base/publisher/publisher.h"
#include "../base/ovlibrary/string.h"

//====================================================================================================
// MonitoringServer
//====================================================================================================
class MonitoringServer
{
public :
    MonitoringServer() = default;

    ~MonitoringServer() = default;

public :
    bool Start(const ov::SocketAddress &address,
               const std::vector<std::shared_ptr<pvd::Provider>> &providers,
                const std::vector<std::shared_ptr<Publisher>> &publishers,
                const std::shared_ptr<Certificate> &certificate = nullptr);

    bool Stop();

    bool Disconnect(const ov::String &app_na, const ov::String &stream_name);

protected:
    bool RequestUrlParsing(const ov::String &request_url,
                            ov::String &file_name,
                            ov::String &file_ext);

    void ProcessRequest(const std::shared_ptr<HttpRequest> &request, const std::shared_ptr<HttpResponse> &response);
    void StateRequest(const std::shared_ptr<HttpResponse> &response);

protected :
    std::shared_ptr<HttpServer> _http_server;
    std::vector<std::shared_ptr<pvd::Provider>> _providers;
    std::vector<std::shared_ptr<Publisher>> _publishers;

};
