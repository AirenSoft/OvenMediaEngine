#pragma once
#include "base/ovlibrary/ovlibrary.h"

#define RTCP_REPORT_BLOCK_SIZE	24

class ReportBlock
{
public:
	bool Parse(const uint8_t *data, size_t data_size);

	uint32_t	GetSrcSsrc(){return _src_ssrc;}
	uint8_t		GetFractionLost(){return _fraction_lost;}
	uint32_t	GetCumulativeLost(){return _cumulative_lost;}
	uint32_t	GetExtentedHighestSequenceNum(){return _extented_highest_sequence_num;}
	uint32_t	GetJitter(){return _jitter;}
	uint32_t	GetLastSr(){return _last_sr;}
	uint32_t	GetDelaySinceLastSr(){return _delay_since_last_sr;}

	void Print();

private:
	uint32_t	_src_ssrc = 0; 
	uint8_t		_fraction_lost = 0;		// 8bits
	uint32_t	_cumulative_lost = 0;	// 24bits
	uint32_t	_extented_highest_sequence_num = 0; // 32bits
	uint32_t	_jitter = 0; // 32bits
	uint32_t	_last_sr = 0; // 32bits (NTP timestamp), If no SR has been received yet, the field is set to zero.
	uint32_t	_delay_since_last_sr = 0; // 32bits, 1/65536 seconds	
};