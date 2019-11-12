#include "rtp_packet.h"
#include "base/ovlibrary/byte_io.h"

RtpPacket::RtpPacket()
{
	_marker = false;
	_payload_type = 0;
	_origin_payload_type = 0;
	_is_fec = false;
	_sequence_number = 0;
	_timestamp = 0;
	_ssrc = 0;
	_payload_offset = FIXED_HEADER_SIZE;
	_payload_size = 0;
	_padding_size = 0;
	_extension_size = 0;

	_data = std::make_shared<ov::Data>();
	// 패킷 최대 크기만큼 넉넉하게 메모리 예약
	_data->Reserve(DEFAULT_MAX_PACKET_SIZE);
	_data->SetLength(FIXED_HEADER_SIZE);
	_buffer = _data->GetWritableDataAs<uint8_t>();

	// 버퍼에 RTP 버전 표기
	_buffer[0] = RTP_VERSION << 6;
}

RtpPacket::RtpPacket(RtpPacket &src)
{
	_marker = src._marker;
	_payload_type = src._payload_type;
	_origin_payload_type = src._origin_payload_type;
	_ssrc = src._ssrc;
	_payload_offset = src._payload_offset;
	_payload_size = src._payload_size;
	_padding_size = src._padding_size;
	_extension_size = src._extension_size;
	_sequence_number = src._sequence_number;
	_timestamp = src._timestamp;

	_data = src._data->Clone();
	_data->SetLength(src._data->GetLength());
	_buffer = _data->GetWritableDataAs<uint8_t>();
}

RtpPacket::~RtpPacket()
{

}

std::shared_ptr<ov::Data> RtpPacket::GetData()
{
	return _data;
}

// Getter
bool RtpPacket::Marker()
{
	return _marker;
}

bool RtpPacket::IsUlpfec()
{
	return _is_fec;
}

uint8_t RtpPacket::PayloadType()
{
	return _payload_type;
}

uint8_t RtpPacket::OriginPayloadType()
{
	return _origin_payload_type;
}

uint16_t RtpPacket::SequenceNumber()
{
	return _sequence_number;
}
uint32_t RtpPacket::Timestamp()
{
	return _timestamp;
}
uint32_t RtpPacket::Ssrc()
{
	return _ssrc;
}
std::vector<uint32_t> RtpPacket::Csrcs()
{
	// 첫번째 바이트의 하위 4bit에 있는 값 추출
	size_t num_csrc = _buffer[0] & 0x0F;
	std::vector<uint32_t> csrcs(num_csrc);
	for (size_t i = 0; i < num_csrc; ++i)
	{
		// 4 bytes char array => 4 bytes uint32_t
		// BIG ENDIAN을 LITTEL ENDIAN으로 변환
		csrcs[i] = ByteReader<uint32_t>::ReadBigEndian(&_buffer[FIXED_HEADER_SIZE + i * 4]);
	}

	return csrcs;
}
uint8_t* RtpPacket::Buffer()
{
	return &_buffer[0];
}

// Setter
void RtpPacket::SetMarker(bool marker_bit)
{
	_marker = marker_bit;

	if (_marker)
	{
		// 맨 앞의 1bit를 1로 만든다.
		_buffer[1] = _buffer[1] | 0x80;
	}
	else
	{
		// 맨 앞의 1bit를 0으로 만든다.
		_buffer[1] = _buffer[1] & 0x7F;
	}
}

void RtpPacket::SetPayloadType(uint8_t payload_type)
{
	_payload_type = payload_type;
	_buffer[1] = (_buffer[1] & 0x80) | payload_type;
}

void RtpPacket::SetUlpfec(bool is_fec, uint8_t origin_payload_type)
{
	_is_fec = is_fec;
	_origin_payload_type = origin_payload_type;
}

void RtpPacket::SetSequenceNumber(uint16_t seq_no)
{
	_sequence_number = seq_no;
	ByteWriter<uint16_t>::WriteBigEndian(&_buffer[2], seq_no);
}

void RtpPacket::SetTimestamp(uint32_t timestamp)
{
	_timestamp = timestamp;
	ByteWriter<uint32_t>::WriteBigEndian(&_buffer[4], timestamp);
}

void RtpPacket::SetSsrc(uint32_t ssrc)
{
	_ssrc = ssrc;
	ByteWriter<uint32_t>::WriteBigEndian(&_buffer[8], ssrc);
}

void RtpPacket::SetCsrcs(const std::vector<uint32_t>& csrcs)
{
	// TODO: Validation 체크
	// 하위 데이터가 없을때만 넣을 수 있다.buffer 정리 때문에, 이거 하려면 다 뒤로 밀어야 함
		// Extention이 없는지?
		// Payload가 없는지?
		// Padding이 없는지?
	// csrcs가 15개를 넘지 않는지? (RFC)
	// buffer reserve가 부족하진 않은지?

	_payload_offset = FIXED_HEADER_SIZE + 4 * csrcs.size();

	// 첫 바이트 하위 4비트에 csrs size 입력
	_buffer[0] = (_buffer[0] & 0xF0) | csrcs.size();

	// _buffer 사이즈 조정
	_data->SetLength(_payload_offset);
	_buffer = _data->GetWritableDataAs<uint8_t>();

	// 4 bytes 짜리 csrc를 buffer에 BIG ENDIAN으로 넣는다.
	size_t offset = FIXED_HEADER_SIZE;
	for (uint32_t csrc : csrcs)
	{
		ByteWriter<uint32_t>::WriteBigEndian(&_buffer[offset], csrc);
		offset += 4;
	}
}

size_t RtpPacket::HeadersSize()
{
	return _payload_offset;
}

size_t RtpPacket::PayloadSize()
{
	return _payload_size;
}

size_t RtpPacket::PaddingSize()
{
	return _padding_size;
}

size_t RtpPacket::ExtensionSize()
{
	return _extension_size;
}

bool RtpPacket::SetPayload(const uint8_t *payload, size_t payload_size)
{
	auto payload_buffer = SetPayloadSize(payload_size);
	if(payload_buffer == nullptr)
	{
		loge("RtpPacket", "Cannot set payload size");
		return false;
	}

	memcpy(payload_buffer, payload, payload_size);

	return true;
}

uint8_t* RtpPacket::SetPayloadSize(size_t size_bytes)
{
	if (_payload_offset + size_bytes > _data->GetCapacity())
	{
		OV_ASSERT(false, "Data capacity must be greater than %ld (offset: %ld + bytes: %ld)", _data->GetCapacity(), _payload_offset, size_bytes, _payload_offset + size_bytes);
		return nullptr;
	}

	_payload_size = size_bytes;
	_data->SetLength(_payload_offset + _payload_size);
	_buffer = _data->GetWritableDataAs<uint8_t>();

	return &_buffer[_payload_offset];
}

uint8_t* RtpPacket::AllocatePayload(size_t size_bytes)
{
	return SetPayloadSize(size_bytes);
}

uint8_t* RtpPacket::Header()
{
	return &_buffer[0];
}

uint8_t* RtpPacket::Payload()
{
	return &_buffer[_payload_offset];
}
