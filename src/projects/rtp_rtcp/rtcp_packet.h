#pragma once

#include <memory>

#include <base/ovlibrary/ovlibrary.h>

class RtcpPacket
{
public:
    RtcpPacket();
    ~RtcpPacket();

	std::shared_ptr<ov::Data> GetData();
};