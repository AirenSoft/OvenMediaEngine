#include "report_block.h"
#include "rtcp_private.h"

#include <base/ovlibrary/byte_io.h>

bool ReportBlock::Parse(const uint8_t *data, size_t data_size)
{
	if(data_size < RTCP_REPORT_BLOCK_SIZE)
	{
		return false;
	}
	
	_src_ssrc = ByteReader<uint32_t>::ReadBigEndian(&data[0]);
	_fraction_lost = ByteReader<uint8_t>::ReadBigEndian(&data[4]);
	_cumulative_lost = ByteReader<uint32_t, 3>::ReadBigEndian(&data[5]);
	_extented_highest_sequence_num = ByteReader<uint32_t>::ReadBigEndian(&data[8]);
	_jitter = ByteReader<uint32_t>::ReadBigEndian(&data[12]);
	_last_sr = ByteReader<uint32_t>::ReadBigEndian(&data[16]);
	_delay_since_last_sr = ByteReader<uint32_t>::ReadBigEndian(&data[20]);

	return true;
}

/*
double RtcpPacket::DelayCalculation(uint32_t lsr, uint32_t dlsr)
{
    if(lsr == 0 || dlsr == 0)
        return 0;

    uint32_t msw = 0;
    uint32_t lsw = 0;

    ov::Clock::GetNtpTime(msw, lsw);

    uint32_t tr = ((msw & 0xFFFF) << 16) | (lsw >> 16);

    if((tr - lsr) <= dlsr)
        return 0;

    return static_cast<double>(tr - lsr - dlsr)/65536.0;
}
*/

void ReportBlock::Print()
{
	logtd("Receiver Report >> source ssrc(%u) fraction lost(%u) cumulative lost(%u) highest sequence(%u) jitter(%u) last sr(%u) delay(%u)",
			GetSrcSsrc(), GetFractionLost(), GetCumulativeLost(), GetExtentedHighestSequenceNum(), GetJitter(), GetLastSr(), GetDelaySinceLastSr());
}