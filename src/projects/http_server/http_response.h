//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "../base/ovlibrary/converter.h"
#include "http_datastructure.h"

class HttpClient;

class HttpResponse : public ov::EnableSharedFromThis<HttpResponse>
{
public:
	friend class HttpClient;

	class IoCallback
	{
	};

	HttpResponse(const std::shared_ptr<ov::ClientSocket> &client_socket);
	~HttpResponse() override = default;

	std::shared_ptr<ov::ClientSocket> GetRemote();
	std::shared_ptr<const ov::ClientSocket> GetRemote() const;

	void SetTlsData(const std::shared_ptr<ov::TlsData> &tls_data);
	std::shared_ptr<ov::TlsData> GetTlsData();

	HttpStatusCode GetStatusCode() const
	{
		return _status_code;
	}

	void SetHttpVersion(const ov::String &http_version)
	{
		_http_version = http_version;
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

	// Enqueue the data into the queue (This data will be sent when SendResponse() is called)
	// Can be used for response with content-length
	bool AppendData(const std::shared_ptr<const ov::Data> &data);
	bool AppendString(const ov::String &string);
	bool AppendFile(const ov::String &filename);

	// Send the data immediately
	// Can be used for response without content-length
	template <typename T>
	bool Send(const T *data)
	{
		return Send(data, sizeof(T));
	}
	virtual bool Send(const void *data, size_t length);
	virtual bool Send(const std::shared_ptr<const ov::Data> &data);

	bool SendChunkedData(const void *data, size_t length);
	bool SendChunkedData(const std::shared_ptr<const ov::Data> &data);

	uint32_t Response();

	bool Close();

	void SetKeepAlive()
	{
		SetHeader("Connection", "keep-alive");
	}

	void SetChunkedTransfer()
	{
		SetHeader("Transfer-Encoding", "chunked");
		_chunked_transfer = true;
	}

	bool IsChunkedTransfer() const
	{
		return _chunked_transfer;
	}

protected:
	uint32_t SendHeaderIfNeeded();
	uint32_t SendResponse();

	std::shared_ptr<ov::ClientSocket> _client_socket;
	std::shared_ptr<ov::TlsData> _tls_data;

	ov::String _http_version = "1.1";

	HttpStatusCode _status_code = HttpStatusCode::OK;
	ov::String _reason = StringFromHttpStatusCode(HttpStatusCode::OK);

	bool _is_header_sent = false;

	std::map<ov::String, ov::String> _response_header;

	// FIXME(dimiden): It is supposed to be synchronized whenever a packet is sent, but performance needs to be improved
	std::recursive_mutex _response_mutex;
	size_t _response_data_size = 0;
	std::vector<std::shared_ptr<const ov::Data>> _response_data_list;

	ov::String _default_value = "";

	bool _chunked_transfer = false;
};
