#pragma once
#include "base/ovlibrary/ovlibrary.h"
#include "rtcp_info.h"

class SenderReport : public RtcpInfo
{
public:
	///////////////////////////////////////////
	// Implement RtcpInfo virtual functions
	///////////////////////////////////////////
	
	bool Parse(const RtcpHeader &header) override
	{
		return true;
	}

	// RtcpInfo must provide raw data
	std::shared_ptr<ov::Data> GetData()
	{
		return nullptr;
	}

	void DebugPrint()
	{
		
	}

	// RtcpInfo must provide packet type
	RtcpPacketType GetPacketType() override
	{
		return RtcpPacketType::SR;
	}
};