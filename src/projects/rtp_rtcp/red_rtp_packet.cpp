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

void RedRtpPacket::SetRed(uint8_t red_payload_type)
{
	// TODO(Getroot): Check validation
	// This function should be used after all header values have been set, before AllocatePayload.

	_red_payload_type = red_payload_type;
	// Replace Payload type with read_payload_type
	_buffer[1] = (_buffer[1] & 0x80) | red_payload_type;

	// Write payload type at the end of the rtp header (Starting point of Payload)
	// The F bit in the RED header should be 0, but the first bit of the Payload type is always 0,
	// so no extra operation is required.
	_buffer[_payload_offset] = _payload_type;
	_payload_offset = _payload_offset + RED_HEADER_SIZE;
}