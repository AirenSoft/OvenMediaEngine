//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2021 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/ovlibrary/ovlibrary.h>
#include "header_fields/rtsp_header_field.h"

#define RTSP_VERSION	"1.0"

enum class RtspMessageType : uint8_t
{
	UNKNOWN = 0,
	REQUEST,
	RESPONSE
};

enum class RtspMethod : uint16_t
{
	UNKNOWN = 0,
	DESCRIBE,
	ANNOUNCE,
	GET_PARAMETER,
	OPTIONS,
	PAUSE,
	PLAY,
	RECORD,
	REDIRECT,
	SETUP,
	SET_PARAMETER,
	TEARDOWN
};

class RtspMessage
{
public:
	// If success, returns RtspMessage and parsed data length
	// If not enough data, returns nullptr and 0
	// If fail, returns nullptr and -1
	// Usage : auto [message, parsed_bytes] = RtspMessage::Parse(~);
	static std::tuple<std::shared_ptr<RtspMessage>, int> Parse(const std::shared_ptr<const ov::Data> &data);

	// For Request Message
	RtspMessage(ov::String method, uint32_t cseq, ov::String request_uri);
	RtspMessage(RtspMethod method, uint32_t cseq, ov::String request_uri);

	// For Response Message
	RtspMessage(uint32_t status_code, uint32_t cseq, ov::String reason_phrase);

	bool AddHeaderField(const std::shared_ptr<RtspHeaderField> &field);
	bool DelHeaderField(ov::String field_name);
	bool SetBody(const std::shared_ptr<ov::Data> &body_data);

	// Header
	std::shared_ptr<ov::Data> GetHeader();
	// Body
	std::shared_ptr<ov::Data> GetBody();
	// Header + Data
	std::shared_ptr<ov::Data> GetMessage();

	// Getter
	uint32_t GetStatusCode();
	ov::String GetReasonPhrase();

private:
	ov::String RtspMethodToString(RtspMethod method);

	// Only use in Parse()
	RtspMessage(){}

	// If success, return parsed data length
	// If not enough data, return 0
	// If fail, return -1
	int ParseInternal(const std::shared_ptr<const ov::Data> &data);

	bool SerializeHeader();

	RtspMessageType _type = RtspMessageType::UNKNOWN;

	// Common Start Line
	ov::String _rtsp_version;

	// Request Line (Start Line)
	ov::String _method_string;
	ov::String _request_uri;

	// Status Line (Response, Start Line)
	uint32_t _status_code;
	ov::String _reason_phrase;

	// Required field
	uint32_t _cseq = 0;

	// field-name : field-value
	std::map<ov::String, std::shared_ptr<RtspHeaderField>>	_header_fields;

	// body
	bool _is_header_data_uptodate = false;
	std::shared_ptr<ov::Data> _header = nullptr;
	std::shared_ptr<ov::Data> _body = nullptr;
};