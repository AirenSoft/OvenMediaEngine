//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "web_socket_response.h"
#include "web_socket_datastructure.h"
#include "../../http_private.h"

#include <unistd.h>
#include <algorithm>

WebSocketResponse::WebSocketResponse(ov::ClientSocket *remote, const std::shared_ptr<HttpRequest> &request, const std::shared_ptr<HttpResponse> &response)
	: _remote(remote),
	  _request(request),
	  _response(response)
{
}

WebSocketResponse::~WebSocketResponse()
{
}

ssize_t WebSocketResponse::Send(const std::shared_ptr<const ov::Data> &data, WebSocketFrameOpcode opcode)
{
	// fragmentation 해야 함

	WebSocketFrameHeader header;

	header.mask = false;

	// RFC6455 - 5.2.  Base Framing Protocol
	//
	//
	ssize_t length = data->GetLength();

	if(length < 0x7D)
	{
		// frame-payload-length    = ( %x00-7D )
		//                         / ( %x7E frame-payload-length-16 )
		//                         / ( %x7F frame-payload-length-63 )
		//                         ; 7, 7+16, or 7+64 bits in length,
		//                         ; respectively
		header.payload_length = static_cast<uint8_t>(length);
	}
	else if(length < 0xFFFF)
	{
		// frame-payload-length-16 = %x0000-FFFF ; 16 bits in length
		header.payload_length = 126;
	}
	else
	{
		// frame-payload-length-63 = %x0000000000000000-7FFFFFFFFFFFFFFF
		//                         ; 64 bits in length
		header.payload_length = 127;
	}

	header.fin = true;
	header.reserved = 0x00;
	header.opcode = static_cast<uint8_t>(opcode);

	_remote->Send(&header);

	if(header.payload_length == 126)
	{
		auto payload_length = ov::HostToNetwork16(static_cast<uint16_t>(length));

		_remote->Send(&payload_length);
	}
	else if(header.payload_length == 127)
	{
		auto payload_length = ov::HostToNetwork64(static_cast<uint64_t>(length));

		_remote->Send(&payload_length);
	}

	ssize_t remained = data->GetLength();
	auto data_to_send = data->GetDataAs<uint8_t>();

	logtd("Trying to send data %ld bytes...", length);

	while(remained > 0L)
	{
		ssize_t to_send = std::min(remained, 1024L * 1024L);
		ssize_t sent = _remote->Send(data_to_send, to_send);

		if(sent < 0L)
		{
			logtw("An error occurred while send data");
			break;
		}

		remained -= sent;
		data_to_send += sent;
	}
}

ssize_t WebSocketResponse::Send(const std::shared_ptr<const ov::Data> &data)
{
	return Send(data, WebSocketFrameOpcode::Binary);
}

ssize_t WebSocketResponse::Send(const ov::String &string)
{
	return Send(string.ToData(false), WebSocketFrameOpcode::Text);
}

void WebSocketResponse::Close()
{
	_remote->Close();
}

ov::String WebSocketResponse::ToString() const
{
	return ov::String::FormatString("<WebSocketResponse: %p, %s>", this, _remote->ToString().CStr());
}