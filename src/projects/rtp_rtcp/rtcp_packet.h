#pragma once

#include <memory>

#include <base/ovlibrary/data.h>

class RtcpPacket
{
public:
    RtcpPacket();
    ~RtcpPacket();

	std::shared_ptr<ov::Data> GetData();
};