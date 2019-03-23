#include "red_rtp_packet.h"

RedRtpPacket::RedRtpPacket()
{

}

RedRtpPacket::RedRtpPacket(RtpPacket &src)
	:RtpPacket(src)
{

}

RedRtpPacket::RedRtpPacket(RedRtpPacket &src)
	:RtpPacket(src)
{
	_red_payload_type = src._red_payload_type;
}

RedRtpPacket::~RedRtpPacket()
{

}

void RedRtpPacket::PackageRed(uint8_t red_payload_type)
{
	// TODO(Getroot): Check validation
	// This function should be used after SetCsrcs, Extensions and before AllocatePayload.

	_payload_offset = _payload_offset + RED_HEADER_SIZE;
	_data->SetLength(_payload_offset);
	_buffer = _data->GetWritableDataAs<uint8_t>();

	_red_payload_type = red_payload_type;
	// Replace Payload type with red_payload_type
	_buffer[1] = (_buffer[1] & 0x80) | red_payload_type;

	// Write payload type at the end of the rtp header
	_buffer[_payload_offset-RED_HEADER_SIZE] = _payload_type;
}

void RedRtpPacket::SetMoreBlockBit(bool f_bit)
{
	// TODO(Getroot): Check validation
	// This function should be used after PackageRed

	_block_bit = f_bit;
	if(_block_bit)
	{
		_buffer[_payload_offset - RED_HEADER_SIZE] |= 0x80;
	}
	else
	{
		_buffer[_payload_offset - RED_HEADER_SIZE] &= 0x7F;
	}
}