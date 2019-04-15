//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "http_request.h"
#include "http_response.h"
#include "http_client.h"
#include "interceptors/default/http_default_interceptor.h"

#include <physical_port/physical_port.h>

// 참고 자료
// RFC7230 - Hypertext Transfer Protocol (HTTP/1.1): Message Syntax and Routing (https://tools.ietf.org/html/rfc7230)
// RFC7231 - Hypertext Transfer Protocol (HTTP/1.1): Semantics and Content (https://tools.ietf.org/html/rfc7231)
// RFC7232 - Hypertext Transfer Protocol (HTTP/1.1): Conditional Requests (https://tools.ietf.org/html/rfc7232)

class HttpServer : protected PhysicalPortObserver
{
public:
	using ClientList = std::map<ov::Socket *, std::shared_ptr<HttpClient>>;
	using ClientIterator = std::function<bool(const std::shared_ptr<HttpClient> &client)>;

	HttpServer() = default;
	~HttpServer() override;

	virtual bool Start(const ov::SocketAddress &address, int sned_buffer_size = 0, int recv_buffer_size = 0);
	virtual bool Stop();

	bool AddInterceptor(const std::shared_ptr<HttpRequestInterceptor> &interceptor);
	bool RemoveInterceptor(const std::shared_ptr<HttpRequestInterceptor> &interceptor);

	// If the iterator returns true, FindClient() will return the client
	ov::Socket *FindClient(ClientIterator iterator);

	// If the iterator returns true, the client will be disconnected
	bool Disconnect(ClientIterator iterator);
	bool Disconnect(std::shared_ptr<HttpClient> client);
	bool Disconnect(const std::shared_ptr<ov::Socket> &remote);

protected:
	// @return 파싱이 성공적으로 되었다면 true를, 데이터가 더 필요하거나 오류가 발생하였다면 false이 반환됨
	ssize_t TryParseHeader(const std::shared_ptr<const ov::Data> &data, const std::shared_ptr<HttpRequest> &request, const std::shared_ptr<HttpResponse> &response);

	std::shared_ptr<HttpClient> FindClient(const std::shared_ptr<ov::Socket> &remote);

	void ProcessData(std::shared_ptr<HttpClient> &client, const std::shared_ptr<const ov::Data> &data);

	//--------------------------------------------------------------------
	// Implementation of PhysicalPortObserver
	//--------------------------------------------------------------------
	void OnConnected(const std::shared_ptr<ov::Socket> &remote) override;
	void OnDataReceived(const std::shared_ptr<ov::Socket> &remote, const ov::SocketAddress &address, const std::shared_ptr<const ov::Data> &data) override;
	void OnDisconnected(const std::shared_ptr<ov::Socket> &remote, PhysicalPortDisconnectReason reason, const std::shared_ptr<const ov::Error> &error) override;

	bool DisconnectInternal(const std::shared_ptr<ov::Socket> &remote);
	bool DisconnectInternal(std::shared_ptr<HttpClient> client);

protected:
	// HttpServer와 연결된 physical port
	std::shared_ptr<PhysicalPort> _physical_port = nullptr;

	std::mutex _client_list_mutex;
	ClientList _client_list;

	std::mutex _interceptor_list_mutex;
	std::vector<std::shared_ptr<HttpRequestInterceptor>> _interceptor_list;
	std::shared_ptr<HttpRequestInterceptor> _default_interceptor = std::make_shared<HttpDefaultInterceptor>();
};
