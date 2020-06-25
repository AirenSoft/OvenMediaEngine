#include "aac_bitstream_analyzer.h"

#include <base/ovlibrary/log.h>

#define OV_LOG_TAG "AACBitstreamAnalyzer"

bool AACBitstreamAnalyzer::IsValidAdtsUnit(const uint8_t *payload)
{
    if(payload == nullptr)
    {
        return false;
    }

    // check syncword
    if( payload[0] == 0xff && (payload[1] & 0xf0) == 0xf0)
    {
        return true;
    }

    return false;
}
