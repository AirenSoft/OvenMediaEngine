//==============================================================================
//
//  MPEGTS Splice Info
//
//  Created by Getroot
//  Copyright (c) 2025 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include <base/ovlibrary/ovlibrary.h>
#include "../descriptors/descriptor.h"

namespace mpegts
{
	enum class SpliceCommandType : uint8_t
	{
		SPLICE_NULL = 0x00,
		SPLICE_SCHEDULE = 0x04,
		SPLICE_INSERT = 0x05,
		SPLICE_TIME_SIGNAL = 0x06,
		SPLICE_BANDWIDTH_RESERVATION = 0x07,
		PRIVATE_COMMAND = 0xFF
	};

	struct SpliceTime
	{
		uint8_t _time_specified_flag = 0;  // 1bit

		// time_specified_flag == 1
		uint8_t _reserved = 0x07;  // 6bits
		int64_t _pts_time = 0;	   // 33bits

		// time_specified_flag == 0
		uint8_t _reserved2 = 0x07;	// 7bits
	};

	struct BreakDuration
	{
		uint8_t _auto_return = 0;  // 1bit
		uint8_t _reserved = 0x07;  // 6bits
		uint64_t _duration = 0;	   // 33bits
	};

	class SpliceInfo
	{
	public:
		std::shared_ptr<ov::Data> Build();

	protected:
		SpliceInfo(SpliceCommandType splice_command_type);
		~SpliceInfo() = default;

		virtual std::shared_ptr<ov::Data> BuildSpliceCommand() = 0;

	private:
		uint8_t _protocol_version = 0;		// 8bits
		uint8_t _encrypted_packet = 0;		// 1bit
		uint8_t _encryption_algorithm = 0;	// 6bits
		uint64_t _pts_adjustment = 0;		// 33bits
		uint8_t _cw_index = 0;				// 8bits
		uint16_t _tier = 0xFFF;				// 12bits

		uint8_t _splice_command_length = 0;	 // 12bits
		uint8_t _splice_command_type = 0;	 // 8bits

		// SpliceCommand - Child class will implement this

		uint8_t _descriptor_loop_length = 0;  // 12bits

		// Does not support yet
		std::vector<std::shared_ptr<Descriptor>> _descriptors;
	};
}  // namespace mpegts