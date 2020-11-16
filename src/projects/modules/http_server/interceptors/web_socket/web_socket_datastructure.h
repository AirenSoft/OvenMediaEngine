//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "../../http_datastructure.h"

#include <base/ovlibrary/ovlibrary.h>
#include <modules/physical_port/physical_port_observer.h>

#include <functional>

class WebSocketClient;

// WebSocket.on('open')
typedef std::function<void(std::shared_ptr<WebSocketClient> &client)> WebSocketConnectCallback;
// WebSocket.on('message')
typedef std::function<void(std::shared_ptr<WebSocketClient> &client, const std::shared_ptr<const ov::Data> &data)> WebSocketMessageCallback;
// WebSocket.on('error')
typedef std::function<void(std::shared_ptr<WebSocketClient> &client, const std::shared_ptr<const ov::Error> &error)> WebSocketErrorCallback;

enum class WebSocketFrameOpcode : uint8_t
{
	// *  %x0 denotes a continuation frame
	Continuation = 0x00,
	// *  %x1 denotes a text frame
	Text = 0x01,
	// *  %x2 denotes a binary frame
	Binary = 0x02,
	// *  %x3-7 are reserved for further non-control frames
	// *  %x8 denotes a connection close
	ConnectionClose = 0x08,
	// *  %x9 denotes a ping
	Ping = 0x09,
	// *  %xA denotes a pong
	Pong = 0x0A,
	// *  %xB-F are reserved for further control frames
};

#pragma pack(push, 1)
// RFC6455 - 5.2. Base Framing Protocol
//  0                   1                   2                   3
//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
// +-+-+-+-+-------+-+-------------+-------------------------------+
// |F|R|R|R| opcode|M| Payload len |    Extended payload length    |
// |I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
// |N|V|V|V|       |S|             |   (if payload len==126/127)   |
// | |1|2|3|       |K|             |                               |
// +-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
// |     Extended payload length continued, if payload len == 127  |
// + - - - - - - - - - - - - - - - +-------------------------------+
// |                               |Masking-key, if MASK set to 1  |
// +-------------------------------+-------------------------------+
// | Masking-key (continued)       |          Payload Data         |
// +-------------------------------- - - - - - - - - - - - - - - - +
// :                     Payload Data continued ...                :
// + - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
// |                     Payload Data continued ...                |
// +---------------------------------------------------------------+
struct WebSocketFrameHeader
{
	// 첫 번째 바이트
	// Opcode:  4 bits
	//
	//    Defines the interpretation of the "Payload data".  If an unknown
	//    opcode is received, the receiving endpoint MUST _Fail the
	//    WebSocket Connection_.  The following values are defined.
	uint8_t opcode : 4;

	// RSV1, RSV2, RSV3:  1 bit each
	//
	//    MUST be 0 unless an extension is negotiated that defines meanings
	//    for non-zero values.  If a nonzero value is received and none of
	//    the negotiated extensions defines the meaning of such a nonzero
	//    value, the receiving endpoint MUST _Fail the WebSocket
	//    Connection_.
	uint8_t reserved : 3;

	// FIN:  1 bit
	//
	//    Indicates that this is the final fragment in a message.  The first
	//    fragment MAY also be the final fragment.
	bool fin : 1;

	// 두 번째 바이트
	// Payload length:  7 bits, 7+16 bits, or 7+64 bits
	//
	//    The length of the "Payload data", in bytes: if 0-125, that is the
	//    payload length.  If 126, the following 2 bytes interpreted as a
	//    16-bit unsigned integer are the payload length.  If 127, the
	//    following 8 bytes interpreted as a 64-bit unsigned integer (the
	//    most significant bit MUST be 0) are the payload length.  Multibyte
	//    length quantities are expressed in network byte order.  Note that
	//    in all cases, the minimal number of bytes MUST be used to encode
	//    the length, for example, the length of a 124-byte-long string
	//    can't be encoded as the sequence 126, 0, 124.  The payload length
	//    is the length of the "Extension data" + the length of the
	//    "Application data".  The length of the "Extension data" may be
	//    zero, in which case the payload length is the length of the
	//    "Application data".
	uint8_t payload_length : 7;

	// Mask:  1 bit
	//
	//    Defines whether the "Payload data" is masked.  If set to 1, a
	//    masking key is present in masking-key, and this is used to unmask
	//    the "Payload data" as per Section 5.3.  All frames sent from
	//    client to server have this bit set to 1.
	bool mask : 1;
};
#pragma pack(pop)

class WebSocketClient;
class WebSocketFrame;

typedef std::function<HttpInterceptorResult(const std::shared_ptr<WebSocketClient> &ws_client)> WebSocketConnectionHandler;
typedef std::function<HttpInterceptorResult(const std::shared_ptr<WebSocketClient> &ws_client, const std::shared_ptr<const WebSocketFrame> &message)> WebSocketMessageHandler;
typedef std::function<void(const std::shared_ptr<WebSocketClient> &ws_client, const std::shared_ptr<const ov::Error> &error)> WebSocketErrorHandler;
typedef std::function<void(const std::shared_ptr<WebSocketClient> &ws_client, PhysicalPortDisconnectReason reason)> WebSocketCloseHandler;
