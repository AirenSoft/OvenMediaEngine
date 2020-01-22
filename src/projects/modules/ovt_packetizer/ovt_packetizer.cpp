//
// Created by getroot on 19. 11. 27.
//

#include <base/ovlibrary/byte_io.h>
#include "ovt_packetizer.h"

OvtPacketizer::OvtPacketizer(const std::shared_ptr<OvtPacketizerInterface> &stream)
{
	_stream = stream;
	_sequence_number = 0;
}

OvtPacketizer::~OvtPacketizer()
{

}



// Packetizing the MediaPacket
bool OvtPacketizer::Packetize(uint64_t timestamp, const std::shared_ptr<MediaPacket> &media_packet)
{
	/**********************************************************************
	 * MediaPacket Serialization
	 **********************************************************************

	 0                   1                   2                   3
	 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
	 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 |                            Track Id							 |
	 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 |                              PTS                              |
	 |           										             |
	 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 |                              DTS                              |
	 |                                                               |
	 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 |                            Duration                           |
	 |                                                               |
	 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 |    MediaType  |    MediaFlag   |            Data Size
	 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                 Data Size            |             Data			 |
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 |                              ...								 |
	 |								...								 |
	 |								...								 |
	 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

	 *********************************************************************/

	ov::Data payload;

	// Header + Data
	payload.SetLength(MEDIA_PACKET_HEADER_SIZE + media_packet->GetData()->GetLength());

	auto buffer = payload.GetWritableDataAs<uint8_t>();

	ByteWriter<uint32_t>::WriteBigEndian(&buffer[0], media_packet->GetTrackId());
	ByteWriter<uint64_t>::WriteBigEndian(&buffer[4], media_packet->GetPts());
	ByteWriter<uint64_t>::WriteBigEndian(&buffer[12], media_packet->GetDts());
	ByteWriter<uint64_t>::WriteBigEndian(&buffer[20], media_packet->GetDuration());
	ByteWriter<uint8_t>::WriteBigEndian(&buffer[28], static_cast<int8_t>(media_packet->GetMediaType()));
	ByteWriter<uint8_t>::WriteBigEndian(&buffer[29], static_cast<int8_t>(media_packet->GetFlag()));
	ByteWriter<uint32_t>::WriteBigEndian(&buffer[30], media_packet->GetData()->GetLength());

	memcpy(&buffer[34], media_packet->GetData()->GetData(), media_packet->GetData()->GetLength());

	size_t max_payload_size = OVT_DEFAULT_MAX_PACKET_SIZE - OVT_FIXED_HEADER_SIZE;
	size_t remain_payload_len = payload.GetLength();
	size_t offset = 0;

	while(remain_payload_len != 0)
	{
		// Serialize
		auto packet = std::make_shared<OvtPacket>();;
		// Session ID should be set in Session Level
		packet->SetSessionId(0);
		packet->SetPayloadType(OVT_PAYLOAD_TYPE_MEDIA_PACKET);
		packet->SetMarker(0);
		packet->SetTimestamp(timestamp);

		if(remain_payload_len > max_payload_size)
		{
			packet->SetPayload(&buffer[offset], max_payload_size);
			offset += max_payload_size;
			remain_payload_len -= max_payload_size;
		}
		else
		{
			// The last packet of group has marker bit.
			packet->SetMarker(true);
			packet->SetPayload(&buffer[offset], remain_payload_len);
			remain_payload_len = 0;
		}

		packet->SetSequenceNumber(_sequence_number++);
		// Callback
		_stream->OnOvtPacketized(packet);
	}

	return true;
}

// For Extension
bool OvtPacketizer::Packetize(uint8_t payload_type, uint64_t timestamp, const std::shared_ptr<ov::Data> &packet)
{
	return false;
}