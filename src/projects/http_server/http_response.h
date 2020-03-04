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

	HttpResponse(std::shared_ptr<HttpClient> client);
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

	// Enqueue the data into the queue (This data will be sent when SendResponse() is called)
	// Can be used for response with content-length
	bool AppendData(const std::shared_ptr<const ov::Data> &data);
	bool AppendString(const ov::String &string);
	bool AppendFile(const ov::String &filename);

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

	std::shared_ptr<HttpClient> _http_client = nullptr;

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
