#include "transport_cc.h"
#include "rtcp_private.h"
#include <base/ovlibrary/byte_io.h>

bool TransportCc::Parse(const RtcpPacket &packet)
{
	const uint8_t *payload = packet.GetPayload();
	size_t payload_size = packet.GetPayloadSize();

	if(payload_size < static_cast<size_t>(TRANSPORT_CC_MIN_SIZE))
	{
		logtd("Payload is too small to parse transport-cc");
		return false;
	}

	// Read Common Feedback
	size_t offset = 0; 
	SetSrcSsrc(ByteReader<uint32_t>::ReadBigEndian(&payload[offset]));
	offset += 4;
	SetMediaSsrc(ByteReader<uint32_t>::ReadBigEndian(&payload[4]));
	offset += 4;
	
	// Read transport-cc feedback
	_base_sequence_number = ByteReader<uint16_t>::ReadBigEndian(&payload[offset]);
	offset += 2;
	_packet_status_count = ByteReader<uint16_t>::ReadBigEndian(&payload[offset]);
	offset += 2;
	_reference_time = ByteReader<uint24_t>::ReadBigEndian(&payload[offset]);
	offset += 3;
	_fb_sequence_number = ByteReader<uint8_t>::ReadBigEndian(&payload[offset]);
	offset += 1;

	for (uint16_t i=0; i<_packet_status_count; i++)
	{
		if (offset + PACKET_CHUNK_BYTES > payload_size)
		{
			logtd("Payload is too small to parse transport-cc");
			return false;
		}

		// Read Packet Chunk
		uint16_t packet_chunk = ByteReader<uint16_t>::ReadBigEndian(&payload[offset]);
		offset += 2;

		// Parse Packet Chunk

		// Run Length Chunk
		//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
		// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		// |0| S |       Run Length        |
		// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		if ((packet_chunk & 0x8000) == 0)
		{
			// Get Symbol (second/third bits)
			uint8_t symbol = (packet_chunk >> 13) & 0x03;
			// Get Run Length (last 13 bits)
			uint16_t run_length = packet_chunk & 0x1FFF;

			uint8_t delta_size = GetDeltaSize(symbol);

			for (uint16_t j=0; j<run_length; j++)
			{
				auto info = std::make_shared<PacketFeedbackInfo>();
				info->_wide_sequence_number = _base_sequence_number + i + j;
				info->_received = (delta_size > 0);
				info->_delta_size = delta_size;

				_packet_feedbacks.push_back(info); 
			}

			i += run_length - 1;
		}
		//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
		// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		// |1|0|       symbol list         |
		// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		// One bit in the symbol list indicates one packet, 
		// 0: not received 1: received, small delta
		else if ((packet_chunk & 0x4000) == 0)
		{
			// loop each bits in symbol list
			for (uint16_t j=0; j<14; j++)
			{
				bool received = ((packet_chunk >> (14 - 1 - j)) & 0x01) == 1;
				uint8_t symbol = received == true ? 1 : 0;
				uint8_t delta_size = GetDeltaSize(symbol);

				auto info = std::make_shared<PacketFeedbackInfo>();
				info->_wide_sequence_number = _base_sequence_number + i + j;
				info->_received = received;
				info->_delta_size = delta_size;
				_packet_feedbacks.push_back(info);
			}

			i += 14 - 1;
		}
		//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
		// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		// |1|1|       symbol list         |
		// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
		// Two bits in the symbol list indicates one packet, 
		else
		{
			// 2bits
			for (uint16_t j=0; j<7; j++)
			{
				uint8_t symbol = (packet_chunk >> (2 * (7 - 1 - j))) & 0x03;
				uint8_t delta_size = GetDeltaSize(symbol);

				auto info = std::make_shared<PacketFeedbackInfo>();
				info->_wide_sequence_number = _base_sequence_number + i + j;
				info->_received = (delta_size > 0);
				info->_delta_size = delta_size;

				_packet_feedbacks.push_back(info); 
			}

			i += 7 - 1;
		}
	}

	// Fill received delta info for packet feedbacks
	for (auto& info : _packet_feedbacks)
	{
		uint16_t received_delta = 0;
		if (info->_delta_size == 1)
		{
			if (payload_size < static_cast<size_t>(offset + 1))
			{
				logtd("Payload is too small to parse transport-cc");
				return false;
			}

			received_delta = ByteReader<uint8_t>::ReadBigEndian(&payload[offset]);
			offset += 1;
		}
		else if (info->_delta_size == 2)
		{
			if (payload_size < static_cast<size_t>(offset + 2))
			{
				logtd("Payload is too small to parse transport-cc");
				return false;
			}

			received_delta = ByteReader<uint16_t>::ReadBigEndian(&payload[offset]);
			offset += 2;
		}
		else 
		{
			// nothing to do
		}

		info->_received_delta = received_delta;
	}

	if (offset != payload_size)
	{
		logtd("Even though parsing transport-cc was completed, the payload is not fully parsed");
		return false;
	}

	return true;
}

// RtcpInfo must provide raw data
std::shared_ptr<ov::Data> TransportCc::GetData() const 
{
	return nullptr;
}

void TransportCc::DebugPrint()
{
	for (auto& info : _packet_feedbacks)
	{
		logtd("PacketFeedbackInfo: reference_time=%u, fb_seq_no=%d, wide_sequence_number=%d, received=%d, delta_size=%d, received_delta=%d", 
		_reference_time, _fb_sequence_number, info->_wide_sequence_number, info->_received, info->_delta_size, info->_received_delta);
	}
}