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
		_splice_time._pts_time = pts < 0 ? 0 : pts & 0x1FFFFFFFF;
	}

	void SpliceInsert::SetDuration(uint64_t duration)
	{
		_duration_flag = 1;
		_break_duration._duration = duration;
	}

	void SpliceInsert::SetAutoReturn(bool auto_return)
	{
		_break_duration._auto_return = auto_return;
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

			if ((_program_splice_flag == 1) && (_splice_immediate_flag == 0))
			{
				stream.WriteBits(1, _splice_time._time_specified_flag);
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

			stream.WriteBytes<uint16_t>(_unique_program_id);
			stream.WriteBytes<uint8_t>(_avail_num);
			stream.WriteBytes<uint8_t>(_avails_expected);
		}

		return stream.GetDataObject();
	}

	std::shared_ptr<SpliceInsert> SpliceInsert::ParseSpliceCommand(const std::shared_ptr<const ov::Data> &data, size_t *command_length)
	{
		BitReader parser(data);

		// Any read past the end of the command clears `ok`, so a command shorter than its flags require is rejected
		bool ok = true;
		auto read = [&](uint8_t bits) -> uint64_t {
			uint64_t value = 0;
			ok = ok && parser.ReadBits<uint64_t>(bits, value);
			return value;
		};

		// `splice_time()`: `time_specified_flag`, then `pts_time` or reserved bits
		auto read_splice_time = [&](SpliceTime *splice_time) {
			splice_time->_time_specified_flag = read(1);
			if (splice_time->_time_specified_flag == 1)
			{
				read(6); // reserved
				splice_time->_pts_time = read(33);
			}
			else
			{
				read(7); // reserved
			}
		};

		auto splice_event_id = static_cast<uint32_t>(read(32));
		auto splice_event_cancel_indicator = static_cast<uint8_t>(read(1));
		read(7); // reserved

		auto splice_insert = std::make_shared<SpliceInsert>(splice_event_id);
		splice_insert->_splice_event_cancel_indicator = splice_event_cancel_indicator;

		if (splice_event_cancel_indicator == 0)
		{
			splice_insert->_out_of_network_indicator = read(1);
			splice_insert->_program_splice_flag = read(1);
			splice_insert->_duration_flag = read(1);
			splice_insert->_splice_immediate_flag = read(1);
			splice_insert->_event_id_compliance_flag = read(1);
			read(3); // reserved

			if ((splice_insert->_program_splice_flag == 1) && (splice_insert->_splice_immediate_flag == 0))
			{
				read_splice_time(&splice_insert->_splice_time);
			}

			// Component splice times are not kept: only program splices are carried downstream
			if (splice_insert->_program_splice_flag == 0)
			{
				const auto component_count = read(8);
				for (uint64_t i = 0; ok && (i < component_count); i++)
				{
					read(8); // `component_tag`

					if (splice_insert->_splice_immediate_flag == 0)
					{
						SpliceTime component_splice_time;
						read_splice_time(&component_splice_time);
					}
				}
			}

			if (splice_insert->_duration_flag == 1)
			{
				splice_insert->_break_duration._auto_return = read(1);
				read(6); // reserved
				splice_insert->_break_duration._duration = read(33);
			}

			splice_insert->_unique_program_id = read(16);
			splice_insert->_avail_num = read(8);
			splice_insert->_avails_expected = read(8);
		}

		if (ok == false)
		{
			return nullptr;
		}

		if (command_length != nullptr)
		{
			*command_length = parser.BytesConsumed();
		}

		return splice_insert;
	}

	ov::String SpliceInsert::ToString() const
	{
		ov::String str;
		str += ov::String::FormatString("Splice Event Id: %u\n", _splice_event_id);
		str += ov::String::FormatString("Splice Event Cancel Indicator: %u\n", _splice_event_cancel_indicator);

		if (_splice_event_cancel_indicator == 0)
		{
			str += ov::String::FormatString("  Out of Network Indicator: %u\n", _out_of_network_indicator);
			str += ov::String::FormatString("  Program Splice Flag: %u\n", _program_splice_flag);
			str += ov::String::FormatString("  Duration Flag: %u\n", _duration_flag);
			str += ov::String::FormatString("  Splice Immediate Flag: %u\n", _splice_immediate_flag);
			str += ov::String::FormatString("  Event Id Compliance Flag: %u\n", _event_id_compliance_flag);

			if ((_program_splice_flag == 1) && (_splice_immediate_flag == 0))
			{
				str += ov::String::FormatString("  Splice Time:\n");
				str += ov::String::FormatString("    Time Specified Flag: %u\n", _splice_time._time_specified_flag);
				if (_splice_time._time_specified_flag == 1)
				{
					str += ov::String::FormatString("    PTS Time: %" PRId64 "\n", _splice_time._pts_time / 90);
				}
			}

			if (_duration_flag == 1)
			{
				str += ov::String::FormatString("  Break Duration:\n");
				str += ov::String::FormatString("    Auto Return: %u\n", _break_duration._auto_return);
				str += ov::String::FormatString("    Duration: %" PRIu64 "\n", _break_duration._duration / 90);
			}

			str += ov::String::FormatString("  Unique Program Id: %u\n", _unique_program_id);
			str += ov::String::FormatString("  Avail Num: %u\n", _avail_num);
			str += ov::String::FormatString("  Avails Expected: %u\n", _avails_expected);
		}

		return str;
	}
}