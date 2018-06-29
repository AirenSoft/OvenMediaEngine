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

#include <memory>

#include <base/ovlibrary/ovlibrary.h>
#include <base/ovsocket/ovsocket.h>

class HttpServer;
class HttpRequest;

class HttpResponse : public ov::EnableSharedFromThis<HttpResponse>
{
public:
	friend class HttpServer;

	HttpResponse(ov::ClientSocket *remote);
	~HttpResponse();

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
	// TODO: 지금 구조에서는 content-length가 확정된 데이터 밖에 보내지 못함. 나중에 필요할 경우, stream 형태로 바꿔야 함
	// (예: 끝을 알 수 없는 데이터 보내기)
	bool AppendData(const std::shared_ptr<const ov::Data> &data);
	bool AppendString(const ov::String &string);
	bool AppendFile(const ov::String &filename);

	bool Response();

	ov::ClientSocket *GetRemote()
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
	bool SendHeaderIfNeeded();
	bool SendResponse();

	HttpStatusCode _status_code;
	ov::String _reason;

	bool _is_header_sent;

	std::map<ov::String, ov::String> _response_header;
	ov::ClientSocket *_remote;

	std::vector<std::shared_ptr<const ov::Data>> _response_data_list;

	ov::String _default_value;
};
