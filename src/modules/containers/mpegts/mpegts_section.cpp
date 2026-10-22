#include "mpegts_section.h"

#include "base/ovlibrary/crc.h"
#include "mpegts_packet.h"
#include "mpegts_private.h"

namespace mpegts
{

	Section::Section(uint16_t pid)
	{
		_pid = pid;
	}

	Section::~Section()
	{
	}

	std::shared_ptr<Section> Section::Build(const PAT &pat)
	{
		auto section = std::make_shared<Section>(static_cast<uint16_t>(WellKnownPacketId::PAT));

		// Table header
		section->_table_id = static_cast<uint8_t>(WellKnownTableId::PROGRAM_ASSOCIATION_SECTION);
		section->_section_syntax_indicator = true;
		section->_section_length = 0;  // later it will be calculated

		section->_pat = std::make_shared<PAT>(pat);

		// Make data
		ov::BitWriter table_data(188);

		// Table data
		table_data.WriteBytes<uint16_t>(section->_pat->_table_id_extension);
		table_data.WriteBits(2, section->_pat->_reserved_bits);
		table_data.WriteBits(5, section->_pat->_version_number);
		table_data.WriteBits(1, section->_pat->_current_next_indicator);
		table_data.WriteBytes<uint8_t>(section->_pat->_section_number);
		table_data.WriteBytes<uint8_t>(section->_pat->_last_section_number);

		// PAT, only one program
		table_data.WriteBytes<uint16_t>(section->_pat->_program_num);
		table_data.WriteBits(3, section->_pat->_reserved_bits2);
		table_data.WriteBits(13, section->_pat->_program_map_pid);

		section->_section_length = table_data.GetDataSize() + 4;  // 4 bytes for CRC

		// Table header
		ov::BitWriter data(188);

		// Table header
		data.WriteBytes<uint8_t>(section->_table_id);
		data.WriteBits(1, section->_section_syntax_indicator);
		data.WriteBits(1, section->_private_bit);
		data.WriteBits(2, section->_reserved_bits);
		data.WriteBits(2, section->_section_length_unused_bits);
		data.WriteBits(10, section->_section_length);
		data.WriteData(table_data.GetData(), table_data.GetDataSize());

		// CRC
		section->_crc = ov::CRC::Crc32Mpeg2(data.GetData(), data.GetDataSize());
		data.WriteBytes<uint32_t>(section->_crc);

		section->_data.Clear();
		section->_data.Append(data.GetData(), data.GetDataSize());

		section->_completed = true;

		return section;
	}

	std::shared_ptr<Section> Section::Build(uint16_t pid, const PMT &pmt)
	{
		auto section = std::make_shared<Section>(pid);

		section->_table_id = static_cast<uint8_t>(WellKnownTableId::PROGRAM_MAP_SECTION);
		section->_section_syntax_indicator = true;
		section->_section_length = 0;  // 13 bytes, later it will be calculated

		section->_pmt = std::make_shared<PMT>(pmt);

		// Make data
		ov::BitWriter table_data(188);

		// Table data
		table_data.WriteBytes<uint16_t>(section->_pmt->_table_id_extension);
		table_data.WriteBits(2, section->_pmt->_reserved_bits1);
		table_data.WriteBits(5, section->_pmt->_version_number);
		table_data.WriteBits(1, section->_pmt->_current_next_indicator);
		table_data.WriteBytes<uint8_t>(section->_pmt->_section_number);
		table_data.WriteBytes<uint8_t>(section->_pmt->_last_section_number);

		// PMT
		table_data.WriteBits(3, section->_pmt->_reserved_bits2);
		table_data.WriteBits(13, section->_pmt->_pcr_pid);
		table_data.WriteBits(4, section->_pmt->_reserved_bits3);
		table_data.WriteBits(2, section->_pmt->_program_info_length_unused_bits);
		table_data.WriteBits(10, section->_pmt->_program_info_length);
		if (section->_pmt->_program_info_length > 0)
		{
			for (auto descriptor : section->_pmt->_program_descriptors)
			{
				auto descriptor_data = descriptor->Build();
				if (descriptor_data != nullptr)
				{
					table_data.WriteData(descriptor_data->GetDataAs<uint8_t>(), descriptor_data->GetLength());
				}
			}
		}

		// ES info
		for (auto es_info : section->_pmt->_es_info_list)
		{
			table_data.WriteBytes<uint8_t>(es_info->_stream_type);
			table_data.WriteBits(3, es_info->_reserved_bits);
			table_data.WriteBits(13, es_info->_elementary_pid);
			table_data.WriteBits(4, es_info->_reserved_bits2);
			table_data.WriteBits(2, es_info->_es_info_length_unused_bits);
			table_data.WriteBits(10, es_info->_es_info_length);

			// ES descriptors
			for (auto descriptor : es_info->_es_descriptors)
			{
				auto descriptor_data = descriptor->Build();
				if (descriptor_data != nullptr)
				{
					table_data.WriteData(descriptor_data->GetDataAs<uint8_t>(), descriptor_data->GetLength());
				}
			}
		}

		section->_section_length = table_data.GetDataSize() + 4;  // 4 bytes for CRC

		ov::BitWriter data(188);

		// Table header
		data.WriteBytes<uint8_t>(section->_table_id);
		data.WriteBits(1, section->_section_syntax_indicator);
		data.WriteBits(1, section->_private_bit);
		data.WriteBits(2, section->_reserved_bits);
		data.WriteBits(2, section->_section_length_unused_bits);
		data.WriteBits(10, section->_section_length);
		data.WriteData(table_data.GetData(), table_data.GetDataSize());

		// CRC
		section->_crc = ov::CRC::Crc32Mpeg2(data.GetData(), data.GetDataSize());
		data.WriteBytes<uint32_t>(section->_crc);

		section->_data.Clear();
		section->_data.Append(data.GetData(), data.GetDataSize());

		section->_completed = true;

		return section;
	}

	std::shared_ptr<Section> Section::Build(uint16_t pid, const std::shared_ptr<SpliceInfo> &splice_info)
	{
		auto section = std::make_shared<Section>(pid);

		section->_table_id = static_cast<uint8_t>(WellKnownTableId::SPLICE_INFO_SECTION);
		section->_section_syntax_indicator = false;

		section->_splice_info = splice_info;
		auto splice_info_data = section->_splice_info->Build();

		section->_section_length = splice_info_data->GetLength() + 4;  // 4 bytes for CRC

		ov::BitWriter data(188);

		// Table header
		data.WriteBytes<uint8_t>(section->_table_id);
		data.WriteBits(1, section->_section_syntax_indicator);
		data.WriteBits(1, section->_private_bit);
		data.WriteBits(2, section->_reserved_bits); // sap_type, 2bits 
		data.WriteBits(12, section->_section_length);
		data.WriteData(splice_info_data->GetDataAs<uint8_t>(), splice_info_data->GetLength());

		ov::Data crc_data(data.GetData(), data.GetDataSize());

		// CRC
		section->_crc = ov::CRC::Crc32Mpeg2(data.GetData(), data.GetDataSize());

		data.WriteBytes<uint32_t>(section->_crc);

		section->_data.Clear();
		section->_data.Append(data.GetData(), data.GetDataSize());
		section->_completed = true;

		return section;
	}

	// return consumed length (including stuff)
	size_t Section::AppendData(const uint8_t *data, uint32_t length)
	{
		size_t consumed_length = 0;

		if (_header_parsed == false)
		{
			auto current_length = _data.GetLength();
			auto need_length = static_cast<size_t>(MPEGTS_TABLE_HEADER_SIZE) - current_length;

			auto append_length = std::min(need_length, static_cast<size_t>(length));
			_data.Append(data, append_length);
			consumed_length = append_length;

			// Table header size is 3 bytes
			if (_data.GetLength() >= MPEGTS_TABLE_HEADER_SIZE)
			{
				BitReader parser(_data.GetWritableDataAs<uint8_t>(), MPEGTS_TABLE_HEADER_SIZE);
				if (ParseTableHeader(&parser) == false)
				{
					logte("Could not parse table header");
					return 0;
				}
			}
			else
			{
				return consumed_length;
			}
		}

		// entire input data is consumed
		if (consumed_length == length)
		{
			return consumed_length;
		}

		// move to follow table header if data is consumed to parse header
		auto input_section_data = data + consumed_length;
		auto input_section_length = length - consumed_length;

		// Header parsed
		auto current_section_length = _data.GetLength() - MPEGTS_TABLE_HEADER_SIZE;
		auto need_length = _section_length - current_section_length;

		auto append_length = std::min(need_length, static_cast<size_t>(input_section_length));
		_data.Append(input_section_data, append_length);
		consumed_length += append_length;

		// check if section is completed
		auto collected_section_length = _data.GetLength() - MPEGTS_TABLE_HEADER_SIZE;
		if (collected_section_length < _section_length)
		{
			// not completed, need more data
			return consumed_length;
		}
		else if (collected_section_length == _section_length)
		{
			// completed
			BitReader parser(_data.GetWritableDataAs<uint8_t>(), _data.GetLength());
			parser.SkipBytes(MPEGTS_TABLE_HEADER_SIZE);
			_failed = (ParseTableData(&parser) == false);

			if (consumed_length < length)
			{
				// Check if remaining data is suffing bytes (0xFF) then skip all stuffing bytes
				uint8_t stuff = *(data + consumed_length);
				if (stuff == 0xFF)
				{
					// remaining data is stuff, all skip
					consumed_length = length;
				}
			}

			return consumed_length;
		}

		return 0;
	}

	std::shared_ptr<PAT> Section::GetPAT()
	{
		return _pat;
	}

	std::shared_ptr<PMT> Section::GetPMT()
	{
		return _pmt;
	}

	std::shared_ptr<SpliceInfo> Section::GetSpliceInfo()
	{
		return _splice_info;
	}

	bool Section::ParseTableHeader(BitReader *parser)
	{
		if (_header_parsed == true)
		{
			// Already parsed
			return false;
		}

		if (parser->BytesRemained() < 3)
		{
			return false;
		}

		_table_id = parser->ReadBytes<uint8_t>();
		// s: section_syntax_indicator
		_section_syntax_indicator = parser->ReadBit();
		_private_bit = parser->ReadBit();
		_reserved_bits = parser->ReadBits<uint8_t>(2);

		// `splice_info_section` carries `sap_type` (any value) here, then a 12-bit `section_length`
		if (_table_id == static_cast<uint8_t>(WellKnownTableId::SPLICE_INFO_SECTION))
		{
			_section_length = parser->ReadBits<uint16_t>(12);
			if (_section_length > MPEGTS_MAX_SPLICE_INFO_SECTION_LENGTH)
			{
				return false;
			}

			_header_parsed = true;

			return true;
		}

		if (_reserved_bits != 0x03)
		{
			// error
			return false;
		}
		// section_length: 12 bits
		_section_length_unused_bits = parser->ReadBits<uint16_t>(2);
		if (_section_length_unused_bits != 0)
		{
			// error
			return false;
		}
		_section_length = parser->ReadBits<uint16_t>(10);
		if (_section_length != parser->BytesRemained() && _section_length > MPEGTS_MAX_PSI_SECTION_LENGTH)
		{
			// error
			return false;
		}

		_header_parsed = true;

		return true;
	}

	bool Section::ParseTableData(BitReader *parser)
	{
		if (_header_parsed == false)
		{
			// header is not parsed
			return false;
		}

		if (_section_length < 4)
		{
			logtw("Could not parse CRC because of not enough data size (section_length: %d, required: 4)", _section_length);
			return false;
		}

		// `CRC_32` is the last 4 bytes of a long-form section, and of `splice_info_section` although it is short-form
		if (_section_syntax_indicator || (_table_id == static_cast<uint8_t>(WellKnownTableId::SPLICE_INFO_SECTION)))
		{
			const auto section_data = _data.GetDataAs<uint8_t>();
			const auto crc_offset = _data.GetLength() - 4;
			_crc = (static_cast<uint32_t>(section_data[crc_offset]) << 24) | (static_cast<uint32_t>(section_data[crc_offset + 1]) << 16) |
				   (static_cast<uint32_t>(section_data[crc_offset + 2]) << 8) | static_cast<uint32_t>(section_data[crc_offset + 3]);

			const auto expected_crc = ov::CRC::Crc32Mpeg2(section_data, crc_offset);
			if (_crc != expected_crc)
			{
				logtd("Could not parse section because of CRC mismatch (table id: %d, CRC: 0x%08X, expected: 0x%08X)", _table_id, _crc, expected_crc);
				_crc_mismatch = true;
				return false;
			}
		}

		switch (_table_id)
		{
			case static_cast<uint8_t>(WellKnownTableId::PROGRAM_ASSOCIATION_SECTION):
				if (ParsePat(parser) == false)
				{
					return false;
				}
				break;
			case static_cast<uint8_t>(WellKnownTableId::PROGRAM_MAP_SECTION):
				if (ParsePmt(parser) == false)
				{
					return false;
				}
				break;
			
			case static_cast<uint8_t>(WellKnownTableId::SPLICE_INFO_SECTION):
				if (ParseSpliceInfo(parser) == false)
				{
					return false;
				}
				break;

			default:
				// Doesn't support
				break;
		}

		if (parser->BytesRemained() < 4)
		{
			logtw("Could not parse CRC because of not enough data size (current: %zu, required: 4)", parser->BytesRemained());
			return false;
		}

		_completed = true;

		return true;
	}

	bool Section::ParsePat(BitReader *parser)
	{
		// not enough data size to parse
		if (parser->BytesRemained() < MPEGTS_MIN_TABLE_DATA_SIZE)
		{
			logtw("Could not parse PAT because of not enough data size (current: %zu, required: %d)", parser->BytesRemained(), MPEGTS_MIN_TABLE_DATA_SIZE);
			return false;
		}

		_pat = std::make_shared<PAT>();

		_pat->_table_id_extension = parser->ReadBytes<uint16_t>();
		_pat->_reserved_bits = parser->ReadBits<uint8_t>(2);
		if (_pat->_reserved_bits != 0x03)
		{
			// error
		}
		_pat->_version_number = parser->ReadBits<uint8_t>(5);
		_pat->_current_next_indicator = parser->ReadBoolBit();
		_pat->_section_number = parser->ReadBytes<uint8_t>();
		_pat->_last_section_number = parser->ReadBytes<uint8_t>();

		// TODO(Getroot): Now assume that PAT and PMT are only in one section
		// It should be changed to support multiple sections
		if (_pat->_section_number != 0 || _pat->_last_section_number != 0)
		{
			logtw("Now it only supports one section for PAT and PMT");
		}

		// Program entries are 4 bytes each and fill the section up to the CRC.
		if ((parser->BytesRemained() - 4) % 4 != 0)
		{
			logtw("Could not parse PAT because the program loop is not a multiple of 4 bytes (%zu)", parser->BytesRemained() - 4);
			return false;
		}

		size_t program_count = 0;

		while (parser->BytesRemained() > 4)
		{
			const auto program_num = parser->ReadBytes<uint16_t>();
			const auto reserved_bits2 = parser->ReadBits<uint8_t>(3);
			const auto pid = parser->ReadBits<uint16_t>(13);

			// `program_number` 0 points to the network PID, not to a PMT
			if (program_num == 0)
			{
				continue;
			}

			if (program_count == 0)
			{
				_pat->_program_num = program_num;
				_pat->_reserved_bits2 = reserved_bits2;
				_pat->_program_map_pid = pid;
			}

			program_count++;
		}

		if (program_count == 0)
		{
			logtw("Could not find a program in PAT");
			return false;
		}

		if (program_count > 1)
		{
			logtw("PAT has %zu programs, but only the first one (program_number: %d) is used", program_count, _pat->_program_num);
		}

		return true;
	}

	// Parses the descriptors that fill exactly `length` bytes, and fails when one runs past them.
	static bool ParseDescriptors(BitReader *parser, size_t length, std::vector<std::shared_ptr<Descriptor>> *descriptors)
	{
		while (length > 0)
		{
			if (length < DESCRIPTOR_HEADER_SIZE)
			{
				logtw("Could not parse descriptor because of not enough data size (remaining: %zu, required: %d)", length, DESCRIPTOR_HEADER_SIZE);
				return false;
			}

			// The second byte of a descriptor is its data length.
			const size_t descriptor_length = DESCRIPTOR_HEADER_SIZE + parser->CurrentPosition()[1];
			if (descriptor_length > length)
			{
				logtw("Could not parse descriptor because it runs past the descriptor loop (remaining: %zu, required: %zu)", length, descriptor_length);
				return false;
			}

			auto descriptor = Descriptor::Parse(parser);
			if (descriptor == nullptr)
			{
				return false;
			}

			descriptors->push_back(descriptor);

			length -= descriptor_length;
		}

		return true;
	}

	bool Section::ParsePmt(BitReader *parser)
	{
		// not enough data size to parse
		if (parser->BytesRemained() < MPEGTS_MIN_TABLE_DATA_SIZE)
		{
			logtw("Could not parse PMT because of not enough data size (current: %zu, required: %d)", parser->BytesRemained(), MPEGTS_MIN_TABLE_DATA_SIZE);
			return false;
		}

		_pmt = std::make_shared<PMT>();

		_pmt->_table_id_extension = parser->ReadBytes<uint16_t>();
		_pmt->_reserved_bits1 = parser->ReadBits<uint8_t>(2);
		if (_pmt->_reserved_bits1 != 0x03)
		{
			// error
		}
		_pmt->_version_number = parser->ReadBits<uint8_t>(5);
		_pmt->_current_next_indicator = parser->ReadBoolBit();
		_pmt->_section_number = parser->ReadBytes<uint8_t>();
		_pmt->_last_section_number = parser->ReadBytes<uint8_t>();

		if (_pmt->_section_number != 0 || _pmt->_last_section_number != 0)
		{
			logtw("Now it only supports one section for PAT and PMT");
		}

		_pmt->_reserved_bits2 = parser->ReadBits<uint8_t>(3);
		_pmt->_pcr_pid = parser->ReadBits<uint16_t>(13);
		_pmt->_reserved_bits3 = parser->ReadBits<uint8_t>(4);
		_pmt->_program_info_length_unused_bits = parser->ReadBits<uint8_t>(2);
		_pmt->_program_info_length = parser->ReadBits<uint16_t>(10);

		if (_pmt->_program_info_length + 4 > parser->BytesRemained())
		{
			logtw("Could not parse program descriptors because of not enough data size (current: %zu, required: %d)", parser->BytesRemained(), _pmt->_program_info_length + 4);
			return false;
		}

		// Descriptors
		if (ParseDescriptors(parser, _pmt->_program_info_length, &_pmt->_program_descriptors) == false)
		{
			return false;
		}

		// es info, remaining bytes excluding CRC(32bits)
		while (parser->BytesRemained() > 4)
		{
			if (parser->BytesRemained() - 4 < 5)
			{
				logtw("Could not parse ES info because of not enough data size (current: %zu, required: 5)", parser->BytesRemained() - 4);
				return false;
			}

			auto es_info = std::make_shared<ESInfo>();

			es_info->_stream_type = parser->ReadBytes<uint8_t>();
			es_info->_reserved_bits = parser->ReadBits<uint8_t>(3);
			es_info->_elementary_pid = parser->ReadBits<uint16_t>(13);
			es_info->_reserved_bits2 = parser->ReadBits<uint8_t>(4);
			es_info->_es_info_length_unused_bits = parser->ReadBits<uint8_t>(2);
			es_info->_es_info_length = parser->ReadBits<uint16_t>(10);

			if (es_info->_es_info_length > parser->BytesRemained() - 4)
			{
				logtw("Could not parse ES descriptors because of not enough data size (current: %zu, required: %d)", parser->BytesRemained() - 4, es_info->_es_info_length);
				return false;
			}

			// Descriptors
			if (ParseDescriptors(parser, es_info->_es_info_length, &es_info->_es_descriptors) == false)
			{
				return false;
			}

			logtt("ES Stream Type: 0x%02X, PID: 0x%04X", es_info->_stream_type, es_info->_elementary_pid);

			_pmt->_es_info_list.push_back(es_info);
		}

		return true;
	}

	bool Section::ParseSpliceInfo(BitReader *parser)
	{
		// not enough data size to parse
		if (parser->BytesRemained() < MPEGTS_MIN_TABLE_DATA_SIZE)
		{
			logtd("Could not parse Splice Info because of not enough data size (current: %zu, required: %d)", parser->BytesRemained(), MPEGTS_MIN_TABLE_DATA_SIZE);
			return false;
		}

		// _splice_info = std::make_shared<SpliceInfo>(SpliceCommandType::SPLICE_INSERT); // SpliceCommandType will be set in Parse function
		// Parse SpiceINfo
		// Protocol Version : 8 bits
		uint8_t protocol_version = parser->ReadBytes<uint8_t>();
		// Encrypted Packet : 1 bit
		bool encrypted_packet = parser->ReadBoolBit();
		// Encryption Algorithm : 6 bits
		uint8_t encryption_algorithm = parser->ReadBits<uint8_t>(6);
		// PTS Adjustment : 33 bits
		uint64_t pts_adjustment = parser->ReadBits<uint64_t>(33);
		// CW Index : 8 bits
		uint8_t cw_index = parser->ReadBytes<uint8_t>();
		// Tier : 12 bits
		uint16_t tier = parser->ReadBits<uint16_t>(12);
		// Splice Command Length : 12 bits
		uint16_t splice_command_length = parser->ReadBits<uint16_t>(12);
		// Splice Command Type : 8 bits
		uint8_t splice_command_type = parser->ReadBytes<uint8_t>();

		// From `splice_command_type` on, an encrypted section holds ciphertext that cannot be decrypted here
		if (encrypted_packet)
		{
			logtd("Could not parse Splice Info because it is encrypted (encryption_algorithm: %d)", encryption_algorithm);
			return false;
		}

		// Only `splice_insert` is handled; `splice_null` heartbeats and other commands are skipped quietly
		if (splice_command_type != static_cast<uint8_t>(SpliceCommandType::SPLICE_INSERT))
		{
			logtd("Unsupported Splice Command Type: %d", splice_command_type);
			return false;
		}

		// The command is followed by `descriptor_loop_length` (2 bytes) and `CRC_32` (4 bytes)
		if (parser->BytesRemained() < 2 + 4)
		{
			logtd("Could not parse Splice Command because of not enough data size (remaining: %zu, required: 6)", parser->BytesRemained());
			return false;
		}

		// 0xFFF is a legacy value to be ignored, and the command then ends where its own syntax ends
		const bool command_length_ignored = (splice_command_length == 0xFFF);
		const size_t command_capacity = command_length_ignored ? (parser->BytesRemained() - 2 - 4) : splice_command_length;

		if (command_capacity + 2 + 4 > parser->BytesRemained())
		{
			logtd("Could not parse Splice Command because it runs past the section (splice_command_length: %d, remaining: %zu)", splice_command_length, parser->BytesRemained());
			return false;
		}

		if (command_capacity == 0)
		{
			logtd("Splice Command Data is empty");
			return false;
		}

		// Splice Command : variable length
		size_t command_length = 0;
		auto splice_insert = SpliceInsert::ParseSpliceCommand(std::make_shared<ov::Data>(parser->CurrentPosition(), command_capacity), &command_length);
		if (splice_insert == nullptr)
		{
			logtd("Could not parse Splice Insert Command");
			return false;
		}
		parser->SkipBytes(command_length_ignored ? command_length : command_capacity);

		// Descriptor Loop Length : 16 bits
		uint16_t descriptor_loop_length = parser->ReadBytes<uint16_t>();
		if (static_cast<size_t>(descriptor_loop_length) + 4 > parser->BytesRemained())
		{
			logtd("Could not parse Splice Descriptors because they run past the section (descriptor_loop_length: %d, remaining: %zu)", descriptor_loop_length, parser->BytesRemained());
			return false;
		}
		parser->SkipBytes(descriptor_loop_length);

		logtt("Splice Info: Protocol Version: %d, Encrypted Packet: %d, Encryption Algorithm: %d, PTS Adjustment: %" PRIu64 ", CW Index: %d, Tier: %d, Splice Command Length: %d, Splice Command Type: %d, Descriptor Loop Length: %d",
			  protocol_version, encrypted_packet, encryption_algorithm, pts_adjustment, cw_index, tier, splice_command_length, splice_command_type, descriptor_loop_length);

		_splice_info = splice_insert;

		_splice_info->SetProtocolVersion(protocol_version);
		_splice_info->SetEncryptedPacket(encrypted_packet);
		_splice_info->SetEncryptionAlgorithm(encryption_algorithm);
		_splice_info->SetPTSAdjustment(pts_adjustment);
		_splice_info->SetCWIndex(cw_index);
		_splice_info->SetTier(tier);
		_splice_info->SetSpliceCommandType(static_cast<SpliceCommandType>(splice_command_type));

		logtt("Parsed Splice Info\n%s", _splice_info->ToString().CStr());

		return true;
	}
	// return true when section is completed
	bool Section::IsCompleted()
	{
		return _completed;
	}

	bool Section::IsFailed()
	{
		return _failed;
	}

	bool Section::HasCrcMismatch()
	{
		return _crc_mismatch;
	}

	// Getter
	uint16_t Section::PID()
	{
		return _pid;
	}

	// Section header
	uint8_t Section::TableId()
	{
		return _table_id;
	}

	bool Section::SectionSyntaxIndicator()
	{
		return _section_syntax_indicator;
	}

	uint16_t Section::SectionLength()
	{
		return _section_length;
	}
}  // namespace mpegts