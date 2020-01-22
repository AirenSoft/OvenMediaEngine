#pragma once

#include <cstdint>

class RtspResponse
{
public:
    RtspResponse(uint16_t status, uint32_t cseq);

private:
    uint16_t status_;
    uint32_t cseq_;
};
