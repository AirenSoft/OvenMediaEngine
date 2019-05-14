//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <memory>

#include <base/application/application.h>
#include <base/media_route/media_route_interface.h>
#include <ice/ice.h>
#include <http_server/http_server.h>
#include <http_server/https_server.h>
#include <http_server/interceptors/http_request_interceptors.h>
#include <media_router/media_route_application.h>
#include <relay/relay.h>
#include "../base/publisher/publisher.h"

class WebConsoleServer : public ov::EnableSharedFromThis<WebConsoleServer>
{
protected:
	struct PrivateToken {};
public:
	WebConsoleServer(const info::Application *application_info, PrivateToken token);
	~WebConsoleServer() override = default;

	static std::shared_ptr<WebConsoleServer> Create(const info::Application *application_info);

	bool Start(const ov::SocketAddress &address);
	bool Stop();

protected:
	bool InitializeServer();

	const info::Application *_application_info;
	cfg::WebConsole _web_console;

	std::shared_ptr<HttpServer> _http_server;
};
