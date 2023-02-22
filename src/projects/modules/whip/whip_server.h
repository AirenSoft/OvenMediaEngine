//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2023 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <modules/http/http.h>

#include "whip_interceptor.h"
#include "whip_observer.h"

class WhipServer : public ov::EnableSharedFromThis<WhipServer>
{
public:
	WhipServer(const cfg::bind::cmm::Webrtc &webrtc_bind_cfg);

	bool Start(
		const std::shared_ptr<WhipObserver> &observer,
		const char *server_name, const char *server_short_name,
		const std::vector<ov::String> &ip_list,
		bool is_port_configured, uint16_t port,
		bool is_tls_port_configured, uint16_t tls_port,
		int worker_count);
	bool Stop();

	bool AppendCertificate(const std::shared_ptr<const info::Certificate> &certificate);
	bool RemoveCertificate(const std::shared_ptr<const info::Certificate> &certificate);

	void SetCors(const info::VHostAppName &vhost_app_name, const std::vector<ov::String> &url_list);
	void EraseCors(const info::VHostAppName &vhost_app_name);

protected:
	bool PrepareForTCPRelay();
	bool PrepareForExternalIceServer();

private:
	std::shared_ptr<WhipInterceptor> CreateInterceptor();
	ov::String GetIceServerLinkValue(const ov::String &URL, const ov::String &username, const ov::String &credential);

	const cfg::bind::cmm::Webrtc _webrtc_bind_cfg;

	std::shared_ptr<WhipObserver> _observer;

	std::recursive_mutex _http_server_list_mutex;
	std::vector<std::shared_ptr<http::svr::HttpServer>> _http_server_list;
	std::vector<std::shared_ptr<http::svr::HttpsServer>> _https_server_list;

	std::vector<ov::String> _link_headers;
	bool _tcp_force = false;

	http::CorsManager _cors_manager;
};