#include "rtx_rtp_packet.h"
#include <base/ovlibrary/byte_io.h>

RtxRtpPacket::RtxRtpPacket(uint32_t rtx_ssrc, uint8_t rtx_payload_type, const RtpPacket &src)
{
	PackageAsRtx(rtx_ssrc, rtx_payload_type, src);
}

bool RtxRtpPacket::PackageAsRtx(uint32_t rtx_ssrc, uint8_t rtx_payload_type, const RtpPacket &src)
{
	SetMarker(src.Marker());
	// replace with rtx payload type
	_origin_payload_type = PayloadType();
	SetPayloadType(rtx_payload_type);
	SetSsrc(rtx_ssrc);
	SetTimestamp(src.Timestamp());

	_payload_offset = src.HeadersSize();
	_payload_size = src.PayloadSize();
	_padding_size = src.PaddingSize();
	_extension_size = src.ExtensionSize();

	// Put OSN
	_origin_seq_no = src.SequenceNumber();
	_payload_offset = _payload_offset + RTX_HEADER_SIZE;
	_data->SetLength(_payload_offset);
	_buffer = _data->GetWritableDataAs<uint8_t>();

	// Write payload type at the end of the rtp header
	ByteWriter<uint16_t>::WriteBigEndian(&_buffer[_payload_offset - RTX_HEADER_SIZE], _origin_seq_no);

	// Copy payload
	SetPayload(src.Payload(), src.PayloadSize());

	return true;
}