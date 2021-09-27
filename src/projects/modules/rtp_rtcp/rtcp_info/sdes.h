#pragma once
#include "base/ovlibrary/ovlibrary.h"
#include "rtcp_info.h"
#include "../rtcp_packet.h"
#include "sdes_chunk.h"

class Sdes : public RtcpInfo
{
public:
	///////////////////////////////////////////
	// Implement RtcpInfo virtual functions
	///////////////////////////////////////////
	bool Parse(const RtcpPacket &packet) override;
	// RtcpInfo must provide raw data
	std::shared_ptr<ov::Data> GetData() const override;
	void DebugPrint() override;

	// RtcpInfo must provide packet type
	RtcpPacketType GetPacketType() const override
	{
		return RtcpPacketType::SDES;
	}

	uint8_t GetCountOrFmt() const override
	{
		return static_cast<uint8_t>(GetChunkCount());
	}

	size_t GetChunkCount() const
	{
		return _sdes_chunk_list.size();
	}

	bool AddChunk(const std::shared_ptr<SdesChunk> &chunk)
	{
		_sdes_chunk_list.push_back(chunk);
		return true;
	}

private:
	std::vector<std::shared_ptr<SdesChunk>>	_sdes_chunk_list;
};