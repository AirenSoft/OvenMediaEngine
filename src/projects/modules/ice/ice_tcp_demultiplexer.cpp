
#include "ice_tcp_demultiplexer.h"
#include "stun/stun_message.h"
#include "stun/channel_data_message.h"

bool IceTcpDemultiplexer::AppendData(const void *data, size_t length)
{	
	_buffer.Append(data, length);
	return ParseData();
}

bool IceTcpDemultiplexer::AppendData(const std::shared_ptr<const ov::Data> &data)
{
	_buffer.Append(data);
	return ParseData();
}

bool IceTcpDemultiplexer::IsAvailablePacket()
{
	return !_packets.empty();
}

std::shared_ptr<IceTcpDemultiplexer::Packet> IceTcpDemultiplexer::PopPacket()
{
	if(IsAvailablePacket() == false)
	{
		return nullptr;
	}

	auto packet = _packets.front();
	_packets.pop();

	return packet;
}

bool IceTcpDemultiplexer::ParseData()
{
	while(_buffer.GetLength() > MINIMUM_PACKET_HEADER_SIZE)
	{
		// Only STUN and TURN Channel should be input packet types to IceTcpDemultiplexer. 
		// If another packet is input, it means a problem has occurred.

		auto type = IcePacketIdentifier::FindPacketType(_buffer);
		IceTcpDemultiplexer::ExtractResult result;

		if(type == IcePacketIdentifier::PacketType::STUN)
		{
			result = ExtractStunMessage();
		}
		else if(type == IcePacketIdentifier::PacketType::TURN_CHANNEL_DATA)
		{
			result = ExtractChannelMessage();
		}
		else
		{
			// Critical error
			return false;
		}

		// success
		if(result == ExtractResult::SUCCESS)
		{
			continue;
		}
		// retry later
		else if(result == ExtractResult::NOT_ENOUGH_BUFFER)
		{
			return true;
		}
		// error
		else if(result == ExtractResult::FAILED)
		{
			return false;
		}
	}

	return true;
}

IceTcpDemultiplexer::ExtractResult IceTcpDemultiplexer::ExtractStunMessage()
{
	ov::ByteStream stream(&_buffer);
	StunMessage message;

	if(message.ParseHeader(stream) == false)
	{
		if(message.GetLastErrorCode() == StunMessage::LastErrorCode::NOT_ENOUGH_DATA)
		{
			// Not enough data, retry later
			return ExtractResult::NOT_ENOUGH_BUFFER;
		}
		else
		{	
			// Invaild data
			return ExtractResult::FAILED;
		}
	}

	uint32_t packet_size = StunMessage::DefaultHeaderLength() + message.GetMessageLength();
	auto data = _buffer.Subdata(0, packet_size);
	auto packet = std::make_shared<IceTcpDemultiplexer::Packet>(IcePacketIdentifier::PacketType::STUN, data);

	_packets.push(packet);
	_buffer.Erase(0, packet_size);

	return ExtractResult::SUCCESS;
}

IceTcpDemultiplexer::ExtractResult IceTcpDemultiplexer::ExtractChannelMessage()
{
	ChannelDataMessage message;

	if(message.LoadHeader(_buffer) == false)
	{
		if(message.GetLastErrorCode() == ChannelDataMessage::LastErrorCode::NOT_ENOUGH_DATA)
		{
			return ExtractResult::NOT_ENOUGH_BUFFER;
		}
		else
		{
			return ExtractResult::FAILED;
		}
	}

	uint32_t packet_size = message.GetPacketLength();
	auto data = _buffer.Subdata(0, packet_size);
	auto packet = std::make_shared<IceTcpDemultiplexer::Packet>(IcePacketIdentifier::PacketType::TURN_CHANNEL_DATA, data);

	_packets.push(packet);
	_buffer.Erase(0, packet_size);

	return ExtractResult::SUCCESS;
}
