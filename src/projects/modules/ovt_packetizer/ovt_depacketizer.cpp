// Created by getroot on 19. 12. 12.
//

#include <base/ovlibrary/byte_io.h>
#include "ovt_depacketizer.h"

#define OV_LOG_TAG		"OvtDepacketizer"

OvtDepacketizer::OvtDepacketizer()
{
	_payload_buffer.Reserve(INIT_PAYLOAD_BUFFER_SIZE);
}

OvtDepacketizer::~OvtDepacketizer()
{

}

bool OvtDepacketizer::AppendPacket(const std::shared_ptr<OvtPacket> &packet)
{
	_payload_buffer.Append(packet->Payload(), packet->PayloadLength());

	// The last packet of MediaPacket
	if(packet->Marker())
	{
		// Validation
		if(_payload_buffer.GetLength() < MEDIA_PACKET_HEADER_SIZE)
		{
			logte("Invalid media packet payload : payload size is less than header size");
			_payload_buffer.Clear();
			return false;
		}

		auto buffer = _payload_buffer.GetDataAs<uint8_t>();
		auto track_id = ByteReader<uint32_t>::ReadBigEndian(&buffer[0]);
		auto pts = ByteReader<uint64_t>::ReadBigEndian(&buffer[4]);
		auto dts = ByteReader<uint64_t>::ReadBigEndian(&buffer[12]);
		auto duration = ByteReader<uint64_t>::ReadBigEndian(&buffer[20]);
		auto media_type = static_cast<cmn::MediaType>(ByteReader<uint8_t>::ReadBigEndian(&buffer[28]));
		auto media_flag = static_cast<MediaPacketFlag>(ByteReader<uint8_t>::ReadBigEndian(&buffer[29]));
		auto data_size = ByteReader<uint32_t>::ReadBigEndian(&buffer[30]);

		if(data_size != _payload_buffer.GetLength() - MEDIA_PACKET_HEADER_SIZE)
		{
			logte("Invalid media packet payload : payload size is invalid");
			_payload_buffer.Clear();
			return false;
		}

		auto media_packet = std::make_shared<MediaPacket>(media_type, track_id,
														_payload_buffer.Subdata(MEDIA_PACKET_HEADER_SIZE),
														pts, dts, duration, media_flag);

		_media_packets.push(media_packet);

		_payload_buffer.Clear();
	}

	return true;
}

bool OvtDepacketizer::IsAvaliableMediaPacket()
{
	return !_media_packets.empty();
}

const std::shared_ptr<MediaPacket> OvtDepacketizer::PopMediaPacket()
{
	if(!IsAvaliableMediaPacket())
	{
		return nullptr;
	}

	auto media_packet = _media_packets.front();
	_media_packets.pop();

	return std::move(media_packet);
}