#include "rtcp_receiver.h"
#include "rtcp_info/sender_report.h"
#include "rtcp_info/receiver_report.h"
#include "rtcp_info/nack.h"

#include "rtcp_info/rtcp_private.h"

bool RtcpReceiver::ParseCompoundPacket(const std::shared_ptr<const ov::Data> &packet)
{
	const uint8_t* buffer = packet->GetDataAs<uint8_t>();
	size_t buffer_size = packet->GetLength();
	size_t offset = 0;
	
	while(offset < buffer_size)
	{
		RtcpPacket rtcp_packet;
		size_t block_size;
		if(rtcp_packet.Parse(buffer + offset, buffer_size - offset, block_size) == false)
		{
			logte("Could not parse RTCP header");
			return false;
		}

		offset += block_size;

		std::shared_ptr<RtcpInfo> info;
		switch(rtcp_packet.GetType())
		{
			case RtcpPacketType::SR:
			{
				info = std::make_shared<SenderReport>();
				break;
			}
			case RtcpPacketType::RR:
			{
				info = std::make_shared<ReceiverReport>();
				break;
			}
			case RtcpPacketType::RTPFB:
			{
				if(rtcp_packet.GetFMT() == static_cast<uint8_t>(RTPFBFMT::NACK))
				{
					info = std::make_shared<NACK>();
				}
				else
				{
					logtd("Does not support RTPFB format : %d", rtcp_packet.GetFMT());
					continue;
				}
				
				break;
			}

			// It will be implemented soon
			case RtcpPacketType::PSFB:
				continue;

			case RtcpPacketType::SDES:
			case RtcpPacketType::BYE:
			case RtcpPacketType::APP:
			case RtcpPacketType::XR:
			default:
				logtd("Does not support RTCP type : %d", static_cast<uint8_t>(rtcp_packet.GetType()));
				continue;
		}

		if(info == nullptr)
		{
			continue;
		}

		if(info->Parse(rtcp_packet) == false)
		{
			return false;
		}

		_rtcp_info_list.push(info);
	}

	return true;
}

bool RtcpReceiver::HasAvailableRtcpInfo()
{
	return _rtcp_info_list.size() > 0;
}

std::shared_ptr<RtcpInfo> RtcpReceiver::PopRtcpInfo()
{
	if(HasAvailableRtcpInfo() == false)
	{
		return nullptr;
	}

	auto info = _rtcp_info_list.front();
	_rtcp_info_list.pop();

	return info;
}