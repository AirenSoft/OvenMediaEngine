#include "rtx_rtp_packet.h"
#include <base/ovlibrary/byte_io.h>

RtxRtpPacket::RtxRtpPacket(uint32_t rtx_ssrc, uint8_t rtx_payload_type, const RtpPacket &src)
	: RtpPacket(src)
{
	PackageAsRtx(rtx_ssrc, rtx_payload_type, src);
}

RtxRtpPacket::RtxRtpPacket(const RtxRtpPacket &src)
	: RtpPacket(src)
{
	_origin_payload_type = src._origin_payload_type;
	_origin_seq_no = src._origin_seq_no;
}

bool RtxRtpPacket::PackageAsRtx(uint32_t rtx_ssrc, uint8_t rtx_payload_type, const RtpPacket &src)
{
	// replace with rtx payload type
	_origin_payload_type = PayloadType();
	SetPayloadType(rtx_payload_type);
	SetSsrc(rtx_ssrc);
	SetTimestamp(src.Timestamp());

	// Put OSN
	_origin_seq_no = src.SequenceNumber();

	_payload_offset = _payload_offset + RTX_HEADER_SIZE;

	if (_data->GetLength() < _payload_offset)
	{
		_data->SetLength(_payload_offset);
		_buffer = _data->GetWritableDataAs<uint8_t>();
	}

	SetOriginalSequenceNumber(_origin_seq_no);
	
	// Rewrite payload
	SetPayload(src.Payload(), src.PayloadSize());

	return true;
}

void RtxRtpPacket::SetOriginalSequenceNumber(uint16_t seq_no)
{
	ByteWriter<uint16_t>::WriteBigEndian(&_buffer[_payload_offset - RTX_HEADER_SIZE], seq_no);
}