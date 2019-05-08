//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "http_datastructure.h"

class HttpRequest;

class HttpResponse : public ov::EnableSharedFromThis<HttpResponse>
{
public:
	friend class HttpClient;

	HttpResponse(HttpRequest *request, std::shared_ptr<ov::ClientSocket> remote);
	~HttpResponse() override = default;

	HttpStatusCode GetStatusCode() const
	{
		return _status_code;
	}

	// reason = default
	void SetStatusCode(HttpStatusCode status_code)
	{
		SetStatusCode(status_code, StringFromHttpStatusCode(status_code));
	}

	// custom reason
	void SetStatusCode(HttpStatusCode status_code, const ov::String &reason)
	{
		_status_code = status_code;
		_reason = reason;
	}

	bool SetHeader(const ov::String &key, const ov::String &value);
	const ov::String &GetHeader(const ov::String &key);

	// message body
	bool AppendData(const std::shared_ptr<const ov::Data> &data);
	bool AppendString(const ov::String &string);
	bool AppendFile(const ov::String &filename);
    bool AppendTlsData(const void *data, size_t length); // HttpClient Call


	template<typename T>
	bool Send(const T *data)
	{
		return Send(data, sizeof(T));
	}

	bool Send(const void *data, size_t length);
	bool Send(const std::shared_ptr<const ov::Data> &data);
    ssize_t Send(const void *data, size_t length, bool &is_retry);

	bool Response();
    ssize_t Response(bool &is_retry);

	std::shared_ptr<ov::ClientSocket> GetRemote()
	{
		return _remote;
	}

	bool IsConnected() const
	{
		return _remote->GetState() == ov::SocketState::Connected;
	}

	bool Close()
	{
		return _remote->Close();
	}

protected:
	void SetTls(const std::shared_ptr<ov::Tls> &tls)
	{
		_tls = tls;
	}

	std::shared_ptr<ov::Tls> GetTls()
	{
		return _tls;
	}

	std::shared_ptr<const ov::Tls> GetTls() const
	{
		return _tls;
	}

	bool SendHeaderIfNeeded();
	bool SendResponse();


	bool MakeResponseData();

	HttpRequest *_request = nullptr;
	std::shared_ptr<ov::ClientSocket> _remote = nullptr;

	std::shared_ptr<ov::Tls> _tls = nullptr;
	std::shared_ptr<const ov::Data> _tls_packet_buffer = nullptr;

	HttpStatusCode _status_code = HttpStatusCode::OK;
	ov::String _reason = StringFromHttpStatusCode(HttpStatusCode::OK);

	bool _is_header_sent = false;

	std::map<ov::String, ov::String> _response_header;

	std::vector<std::shared_ptr<const ov::Data>> _response_data_list;

	ov::String _default_value = "";


    std::shared_ptr<ov::Data> _http_response_data = nullptr; // header + body
    std::shared_ptr<ov::Data> _http_tls_response_data = nullptr; // (header + body)tls encoded
    bool _is_tls_write_save = false;


};
