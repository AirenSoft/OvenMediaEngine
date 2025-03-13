//==============================================================================
//
//  MPEGTS SpliceInsert
//
//  Created by Getroot
//  Copyright (c) 2025 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include <base/ovlibrary/ovlibrary.h>
#include "mpegts_splice_info.h"

namespace mpegts
{
	class SpliceInsert : public SpliceInfo
	{
	public:
		SpliceInsert(uint32_t event_id);
		~SpliceInsert() = default;

		// true : scte35-out, false : scte35-in
		void SetOutofNetworkIndicator(bool out_of_network);
		void SetPTS(int64_t pts);
		void SetDuration(uint64_t duration);

	private:
		std::shared_ptr<ov::Data> BuildSpliceCommand() override;

		uint32_t _splice_event_id = 0;				 // 32bits
		uint8_t _splice_event_cancel_indicator = 0;	 // 1bit
		uint8_t _reserved = 0x07;					 // 7bits

		// cancel indicator == 0
			uint8_t _out_of_network_indicator = 0;	// 1bit, 1: scte35-out 0: scte35-in
			uint8_t _program_splice_flag = 0;		// 1bit
			uint8_t _duration_flag = 0;				// 1bit
			uint8_t _splice_immediate_flag = 0;		// 1bit
			uint8_t _event_id_compliance_flag = 0;	// 1bit
			uint8_t _reserved2 = 0x03;				// 3bits
			// (program_splice_flag == ‘1’) && (splice_immediate_flag == ‘0’)
				SpliceTime _splice_time;				// 32bits

			// duration_flag == 1
			BreakDuration _break_duration;

		uint16_t _unique_program_id = 0;  // 16bits
		uint8_t _avail_num = 0;			  // 8bits
		uint8_t _avails_expected = 0;	  // 8bits
	};
}