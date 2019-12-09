//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "web_socket_frame.h"
#include "../../http_private.h"

#include <algorithm>

WebSocketFrame::WebSocketFrame()
	: _remained_length(0L),
	  _total_length(0L),

	  _last_status(WebSocketFrameParseStatus::Prepare)
{
}

WebSocketFrame::~WebSocketFrame()
{
}

ssize_t WebSocketFrame::Process(const std::shared_ptr<const ov::Data> &data)
{
	switch(_last_status)
	{
		case WebSocketFrameParseStatus::Prepare:
		case WebSocketFrameParseStatus::Parsing:
			break;

		case WebSocketFrameParseStatus::Completed:
		case WebSocketFrameParseStatus::Error:
			// 이미 파싱이 완료되었거나 오류가 발생한 상태에서, 데이터가 또 들어오면 오류로 간주
			OV_ASSERT2(false);
			_last_status = WebSocketFrameParseStatus::Error;
			return -1L;
	}

	ov::ByteStream stream(data.get());

	ssize_t header_length = 0L;

	if(_last_status == WebSocketFrameParseStatus::Prepare)
	{
		header_length = ProcessHeader(stream);

		if(_last_status != WebSocketFrameParseStatus::Parsing)
		{
			return header_length;
		}
	}

	// 남은 데이터와 websocket frame 크기 중, 작은 크기를 구해 데이터를 누적함
	size_t length = std::min(static_cast<uint64_t>(stream.Remained()), _remained_length);

	if(length > 0L)
	{
		// 잔여 데이터가 있다면 payload에 추가
		if(_payload->Append(stream.GetRemainData()->GetData(), length) == false)
		{
			return -1L;
		}
	}

	_remained_length -= length;

	if(_payload->GetLength() == _total_length)
	{
		OV_ASSERT2(_remained_length == 0L);

		// masking 처리
		if(_header.mask)
		{
			size_t payload_length = _payload->GetLength();

			// 빠른 처리를 위해 8의 배수로 처리함 (mask가 있을 경우, 데이터 할당 시 이미 8의 배수로 할당해놓음)

			// masking은 반드시 4byte여야 함
			OV_ASSERT2(sizeof(_frame_masking_key) == 4);

			uint64_t mask = static_cast<uint64_t>(_frame_masking_key) << (sizeof(_frame_masking_key) * 8) | static_cast<uint64_t>(_frame_masking_key);

			// 8의 배수로 만듦
			size_t count = ((payload_length + 7L) / 8L);
			_payload->SetLength(static_cast<size_t>(count * 8L));

			auto current = _payload->GetWritableDataAs<uint64_t>();

			for(size_t index = 0; index < count; index++)
			{
				*current ^= mask;
				current++;
			}

			_payload->SetLength(payload_length);
		}

		logtd("The frame is finished: %s", ToString().CStr());

		_last_status = WebSocketFrameParseStatus::Completed;
		return header_length + length;
	}

	logtd("Data received: %ld / %ld (remained: %ld)", _payload->GetLength(), _total_length, _remained_length);

	OV_ASSERT(_payload->GetLength() <= _total_length, "Invalid payload length: payload length (%ld) must less equal than total length (%ld)", _payload->GetLength(), _total_length);

	return header_length + length;
}

WebSocketFrameParseStatus WebSocketFrame::GetStatus() const noexcept
{
	return _last_status;
}

const WebSocketFrameHeader &WebSocketFrame::GetHeader()
{
	return _header;
}

ssize_t WebSocketFrame::ProcessHeader(ov::ByteStream &stream)
{
	ssize_t before_offset = stream.GetOffset();
	ssize_t read_count = stream.Read(reinterpret_cast<char *>(&_header) + _header_read_bytes, sizeof(_header) - _header_read_bytes);

	if(read_count >= 0)
	{
		_header_read_bytes += read_count;
	}

	if(_header_read_bytes != sizeof(_header))
	{
		// Not enough data to process
		return read_count;
	}

	// extensions 사용 여부
	// TODO: 일단은 사용하지 않는 것으로 간주
	bool extensions = false;

	if((extensions == false) && (_header.reserved != 0x00))
	{
		OV_ASSERT(false, "Invalid reserved value: %d (expected: %d)", _header.reserved, 0x00);
		_last_status = WebSocketFrameParseStatus::Error;
		return -1L;
	}

	_total_length = 0L;
	_remained_length = 0L;

	// 길이 계산
	//
	// RFC6455 - 5.2.  Base Framing Protocol
	//
	// Payload length:  7 bits, 7+16 bits, or 7+64 bits
	//
	//     The length of the "Payload data", in bytes: if 0-125, that is the
	//     payload length.  If 126, the following 2 bytes interpreted as a
	//     16-bit unsigned integer are the payload length.  If 127, the
	//     following 8 bytes interpreted as a 64-bit unsigned integer (the
	//     most significant bit MUST be 0) are the payload length.  Multibyte
	//     length quantities are expressed in network byte order.  Note that
	//     in all cases, the minimal number of bytes MUST be used to encode
	//     the length, for example, the length of a 124-byte-long string
	//     can't be encoded as the sequence 126, 0, 124.  The payload length
	//     is the length of the "Extension data" + the length of the
	//     "Application data".  The length of the "Extension data" may be
	//     zero, in which case the payload length is the length of the
	//     "Application data".
	switch(_header.payload_length)
	{
		case 126:
		{
			// 126이면, 다음 이어서 오는 16bit가 payload length
			uint16_t extra_length;

			if(stream.Read(&extra_length) == 1)
			{
				_total_length = ov::NetworkToHost16(extra_length);
				_remained_length = _total_length;
			}
			else
			{
				OV_ASSERT(false, "Could not read payload length");
				_last_status = WebSocketFrameParseStatus::Error;
				return -1L;
			}

			break;
		}

		case 127:
		{
			// 127이면, 다음 이어서 오는 64bit가 payload length
			uint64_t extra_length;

			if(stream.Read(&extra_length) == 1)
			{
				_total_length = ov::NetworkToHost64(extra_length);
				_remained_length = _total_length;
			}
			else
			{
				OV_ASSERT(false, "Could not read payload length");
				_last_status = WebSocketFrameParseStatus::Error;
				return -1L;
			}

			break;
		}

		default:
			_total_length = _header.payload_length;
			_remained_length = _total_length;
	}

	// masking을 해야한다면, 나중에 속도 최적화를 위해 8의 배수로 masking을 처리하는데, 이 때 버퍼 재할당이 일어나지 않도록 8의 배수로 미리 맞춰놓음
	ssize_t total_length = _header.mask ? (_total_length / 8L) * 8L : _total_length;
	_payload = std::make_shared<ov::Data>(total_length);

	if(_header.mask)
	{
		if(stream.Read(&_frame_masking_key) != 1)
		{
			OV_ASSERT(false, "Could not read masking key");
			_last_status = WebSocketFrameParseStatus::Error;
			return -1L;
		}
	}

	_last_status = WebSocketFrameParseStatus::Parsing;
	return stream.GetOffset() - before_offset;
}

ov::String WebSocketFrame::ToString() const
{
	return ov::String::FormatString(
		"<WebSocketFrame: %p, Fin: %s, Opcode: %d, Mask: %s, Payload length: %ld",
		this,
		(_header.fin ? "True" : "False"),
		_header.opcode,
		(_header.mask ? "True" : "False"),
		_total_length
	);
}
