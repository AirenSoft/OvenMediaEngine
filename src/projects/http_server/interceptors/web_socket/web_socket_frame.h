//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "./web_socket_datastructure.h"

enum class WebSocketFrameParseStatus
{
	Prepare,
	Parsing,
	Completed,
	Error,
};

class WebSocketFrame
{
public:
	WebSocketFrame();
	virtual ~WebSocketFrame();

	void Reset()
	{
		::memset(&_header, 0, sizeof(_header));
		_total_length = 0L;

		_last_status = WebSocketFrameParseStatus::Prepare;

		_payload->Clear();
	}

	const std::shared_ptr<const ov::Data> GetPayload() const noexcept
	{
		return _payload;
	}

	ssize_t Process(const std::shared_ptr<const ov::Data> &data);
	WebSocketFrameParseStatus GetStatus() const noexcept;

	const WebSocketFrameHeader &GetHeader();

	ov::String ToString() const;

protected:
	ssize_t ProcessHeader(ov::ByteStream &stream);

	WebSocketFrameHeader _header;
	int _header_read_bytes = 0;

	uint64_t _remained_length;
	uint64_t _total_length;
	uint32_t _frame_masking_key;

	WebSocketFrameParseStatus _last_status;

	std::shared_ptr<ov::Data> _payload;
};
