//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2025 AirenSoft. All rights reserved.
//
//==============================================================================

#include "mpegts_splice_insert.h"

namespace mpegts
{
	SpliceInsert::SpliceInsert(uint32_t event_id)
		: SpliceInfo(SpliceCommandType::SPLICE_INSERT)
	{
		_splice_event_id = event_id;
	}

	void SpliceInsert::SetOutofNetworkIndicator(bool out_of_network)
	{
		_out_of_network_indicator = out_of_network;
	}

	void SpliceInsert::SetPTS(int64_t pts)
	{
		_splice_time._time_specified_flag = 1;

		// pts_time: 33bits, 90kHz / max : 0x1FFFFFFFF
		_splice_time._pts_time = pts < 0 ? 0 : pts % 0x1FFFFFFFF;
	}

	void SpliceInsert::SetDuration(uint64_t duration)
	{
		_duration_flag = 1;
		_break_duration._duration = duration;
	}

	std::shared_ptr<ov::Data> SpliceInsert::BuildSpliceCommand()
	{
		ov::BitWriter stream(188);

		stream.WriteBytes<uint32_t>(_splice_event_id);
		stream.WriteBits(1, _splice_event_cancel_indicator);
		stream.WriteBits(7, _reserved);

		if (_splice_event_cancel_indicator == 0)
		{
			stream.WriteBits(1, _out_of_network_indicator);
			stream.WriteBits(1, _program_splice_flag);
			stream.WriteBits(1, _duration_flag);
			stream.WriteBits(1, _splice_immediate_flag);
			stream.WriteBits(1, _event_id_compliance_flag);
			stream.WriteBits(3, _reserved2);

			if (_program_splice_flag == 1)
			{
				stream.WriteBits(8, _splice_time._time_specified_flag);
				if (_splice_time._time_specified_flag == 1)
				{
					stream.WriteBits(6, _splice_time._reserved);
					stream.WriteBits(33, _splice_time._pts_time);
				}
				else
				{
					stream.WriteBits(7, _splice_time._reserved2);
				}
			}

			if (_duration_flag == 1)
			{
				stream.WriteBits(1, _break_duration._auto_return);
				stream.WriteBits(6, _break_duration._reserved);
				stream.WriteBits(33, _break_duration._duration);
			}
		}

		return stream.GetDataObject();
	}
}