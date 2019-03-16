//
// Created by getroot on 19. 3. 14.
//

#include "ulpfec_generator.h"

constexpr size_t 	kFecHeaderSize					= 10;
constexpr size_t 	kFecLevelHeaderSizeLbitClear 	= 4;
constexpr size_t	kFecLevelHeaderSizeLbitSet		= 8;
constexpr size_t 	kUlpfecMaxMediaPacketsLbitClear	= 16;
constexpr size_t 	kUlpfecMaxMediaPacketsLbitSet	= 48;

RedPacket::RedPacket(size_t length)
{

}

RedPacket::~RedPacket()
{

}

void RedPacket::CreateHeader(const uint8_t* rtp_header, size_t header_length, int red_payload_type, int payload_type)
{

}

void RedPacket::SetSeqNum(int seq_num)
{

}

void RedPacket::AssignPayload(const uint8_t* payload, size_t length)
{

}

void RedPacket::ClearMarkerBit()
{

}

uint8_t* RedPacket::data() const
{

}

size_t RedPacket::length() const
{

}


UlpfecGenerator::UlpfecGenerator()
{
	_prev_fec_sent = false;
	_protection_rate = -1;
	_new_protection_rate = -1;
}

UlpfecGenerator::~UlpfecGenerator()
{

}

void UlpfecGenerator::SetProtectRate(int32_t protection_rate)
{
	// Minimum value of protection_rate is 10%
	if(protection_rate !=0 && protection_rate < 10)
	{
		protection_rate = 10;
	}

	_new_protection_rate = protection_rate;

	// First setting
	if(_protection_rate == -1)
	{
		_protection_rate = _new_protection_rate;
	}
}

bool UlpfecGenerator::AddRtpPacketAndGenerateFec(std::unique_ptr<RtpPacket> packet)
{
	if(_protection_rate <= 0)
	{
		return false;
	}

	_media_packets.push_back(packet);

	//TODO(Getroot): Improve packet selection algorithm, using Bursty Mask or Random Mask

	if(_media_packets.size() >= (100/_protection_rate))
	{
		EncodeUlpfecWithRed();

		if(!packet->Marker())
		{
			_prev_fec_sent = true;
		}
	}
	else if(packet->Marker() && _prev_fec_sent)
	{
		EncodeUlpfecWithRed();

		_prev_fec_sent = false;

		// The new protection rate is applied from the first packet in the new frame.
		_protection_rate = _new_protection_rate;
	}

	return true;
}
bool UlpfecGenerator::IsAvailableFecPackets() const
{

}

std::unique_ptr<RedPacket> UlpfecGenerator::GetUlpfecPacketsAsRed(int red_payload_type, int ulpfec_payload_type, uint16_t first_seq_num)
{

}

bool UlpfecGenerator::EncodeUlpfecWithRed()
{





	return true;
}




bool UlpfecGenerator::GeneratePayload()
{
	bool fist_media_packet = true;

	for(auto const &media_packet : _media_packets)
	{
		media_packet->HeadersSize();
		media_packet->PayloadSize();
		size_t fec_packet_length = kFecHeaderSize + media_packet->PayloadSize();

		// First Media Packet
		if(_fec_packet->Length() < fec_packet_length)
		{
			_fec_packet->SetLength(fec_packet_length);
		}


	}
}