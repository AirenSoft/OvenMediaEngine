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
	_data->Reserve(RTP_DEFAULT_MAX_PACKET_SIZE);
	_data->SetLength(FIXED_HEADER_SIZE);
	_buffer = _data->GetWritableDataAs<uint8_t>();

	_buffer[0] = RTP_VERSION << 6;

	_created_time = std::chrono::system_clock::now();

	_is_available = true;
}

RtpPacket::RtpPacket(const std::shared_ptr<const ov::Data> &data)
{
	_is_available = Parse(data);
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

	_created_time = std::chrono::system_clock::now();

	_is_available = true;
}

RtpPacket::~RtpPacket()
{

}

ov::String RtpPacket::Dump()
{
	if(_is_available == false)
	{
		return ov::String::FormatString("Invalid packet");
	}

	return ov::String::FormatString("RTP Packet - size(%u) p(%d) x(%d) cc(%d) m(%d) pt(%d) seq_no(%d) timestamp(%u) ssrc(%u) extension_size(%d) payload_offset(%d), payload_size(%u) padding_size(%u)", _data->GetLength(), _has_padding, _has_extension, _cc, _marker, _payload_type, _sequence_number, _timestamp, _ssrc, _extension_size, _payload_offset, _payload_size, _padding_size);
}

bool RtpPacket::Parse(const std::shared_ptr<const ov::Data> &data)
{
	if(data->GetLength() < FIXED_HEADER_SIZE)
	{
		// Wrong data
		return false;
	}

	auto buffer = data->GetDataAs<uint8_t>();
	auto buffer_size = data->GetLength();

	uint8_t version = buffer[0] >> 6;
	if(version != RTP_VERSION)
	{
		// This data is not RTP packet
		return false;
	}

	_has_padding = (buffer[0] & 0x20) != 0;
	_has_extension = (buffer[0] & 0x10) != 0;
	_cc = buffer[0] & 0x0f;

	// It is specific values only for OME
	_is_fec = false;
	_origin_payload_type = 0;

	// Marker
	_marker = (buffer[1] & 0x80) != 0;
	// PT
	_payload_type = buffer[1] & 0x7f;
	// Sequence Number
	_sequence_number = ByteReader<uint16_t>::ReadBigEndian(&buffer[2]);
	// Timestamp
	_timestamp = ByteReader<uint32_t>::ReadBigEndian(&buffer[4]);
	// SSRC
	_ssrc = ByteReader<uint32_t>::ReadBigEndian(&buffer[8]);

	//TODO(Getroot): Parse CSRC
	_payload_offset = FIXED_HEADER_SIZE + (_cc * 4);
	
	if(_has_padding)
	{
		_padding_size = buffer[buffer_size -1];
		if(_padding_size == 0)
		{
			return false;
		}
	}
	else
	{
		_padding_size = 0;
	}

	_extension_size = 0;
	if(_has_extension)
	{
		/*
		https://tools.ietf.org/html/rfc3550#section-5.3.1

		16-bit length field that counts the number of 32-bit words in the extension, 
		excluding the four-octet extension header

		0                   1                   2                   3
		0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		|      defined by profile       |           length              |
		+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		|                        header extension                       |
		|                             ....                              |
		*/
		size_t extension_offset = _payload_offset + 4;
		if(extension_offset > buffer_size)
		{
			return false;
		}

		uint16_t extension_profile = ByteReader<uint16_t>::ReadBigEndian(&buffer[_payload_offset]);
		_extension_size = ByteReader<uint16_t>::ReadBigEndian(&buffer[_payload_offset + 2]) * 4;

		if(extension_offset + _extension_size > buffer_size)
		{
			return false;
		}

		// https://tools.ietf.org/html/rfc8285
		// A General Mechanism for RTP Header Extensions
		// TODO(Getroot): Extract extensions
		
		_payload_offset = extension_offset + _extension_size;
	}

	if(_payload_offset + _padding_size > buffer_size)
	{
		return false;
	}

	_payload_size = buffer_size - _payload_offset - _padding_size;

	// Full data
	_data = data->Clone();
	_buffer = _data->GetWritableDataAs<uint8_t>();

	_created_time = std::chrono::system_clock::now();

	return true;
}

std::shared_ptr<ov::Data> RtpPacket::GetData()
{
	return _data;
}

// Getter
bool RtpPacket::Marker() const
{
	return _marker;
}

bool RtpPacket::IsUlpfec() const
{
	return _is_fec;
}

uint8_t RtpPacket::PayloadType() const
{
	return _payload_type;
}

uint8_t RtpPacket::OriginPayloadType() const
{
	return _origin_payload_type;
}

uint16_t RtpPacket::SequenceNumber() const
{
	return _sequence_number;
}
uint32_t RtpPacket::Timestamp() const
{
	return _timestamp;
}
uint32_t RtpPacket::Ssrc() const
{
	return _ssrc;
}
std::vector<uint32_t> RtpPacket::Csrcs() const
{
	// Extract the value in the lower 4 bits of the first byte
	size_t num_csrc = _buffer[0] & 0x0F;
	std::vector<uint32_t> csrcs(num_csrc);
	for (size_t i = 0; i < num_csrc; ++i)
	{
		// 4 bytes char array => 4 bytes uint32_t
		csrcs[i] = ByteReader<uint32_t>::ReadBigEndian(&_buffer[FIXED_HEADER_SIZE + i * 4]);
	}

	return csrcs;
}
uint8_t* RtpPacket::Buffer() const
{
	return &_buffer[0];
}

// Setter
void RtpPacket::SetMarker(bool marker_bit)
{
	_marker = marker_bit;

	if (_marker)
	{
		// Make the first 1 bit 1
		_buffer[1] = _buffer[1] | 0x80;
	}
	else
	{
		// Make the first 1 bit 0.
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
	// TODO: Validation check
	// can only be inserted when there is no sub-data. Because of buffer cleanup, you have to push it all back to do this
	// Is there no Extention?
	// Is there no payload?
	// Is there no Padding?
	// Is there no more than 15 csrcs? (RFC)
	// Isn't there insufficient buffer reserve?

	_payload_offset = FIXED_HEADER_SIZE + 4 * csrcs.size();

	// Enter csrs size in the lower 4 bits of the first byte
	_cc = csrcs.size();
	_buffer[0] = (_buffer[0] & 0xF0) | _cc;

	// Adjust _buffer size
	_data->SetLength(_payload_offset);
	_buffer = _data->GetWritableDataAs<uint8_t>();

	size_t offset = FIXED_HEADER_SIZE;
	for (uint32_t csrc : csrcs)
	{
		ByteWriter<uint32_t>::WriteBigEndian(&_buffer[offset], csrc);
		offset += 4;
	}
}

size_t RtpPacket::HeadersSize() const
{
	return _payload_offset;
}

size_t RtpPacket::PayloadSize() const
{
	return _payload_size;
}

size_t RtpPacket::PaddingSize() const
{
	return _padding_size;
}

size_t RtpPacket::ExtensionSize() const
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
		OV_ASSERT(false, "Data capacity must be greater than %ld (packet : %ld)", _data->GetCapacity(), _payload_offset + size_bytes);
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

uint8_t* RtpPacket::Header() const
{
	return &_buffer[0];
}

uint8_t* RtpPacket::Payload() const
{
	return &_buffer[_payload_offset];
}

std::chrono::system_clock::time_point RtpPacket::GetCreatedTime()
{
	return _created_time;
}