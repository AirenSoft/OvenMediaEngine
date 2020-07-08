#include "mpegts_section.h"
#include "base/ovlibrary/crc.h"

#define OV_LOG_TAG	"MpegTsSection"

namespace mpegts
{

	Section::Section(uint16_t pid)
	{
		_pid = pid;
	}

	Section::~Section()
	{

	}

	// return consumed length (including stuff)
	size_t Section::AppendData(const uint8_t *data, uint32_t length)
	{
		size_t consumed_length = 0;

		if(_header_parsed == false)
		{
			auto current_length = _data.GetLength();
			auto need_length = static_cast<size_t>(MPEGTS_TABLE_HEADER_SIZE) - current_length;

			auto append_length = std::min(need_length, static_cast<size_t>(length));
			_data.Append(data, append_length);
			consumed_length = append_length;

			// Table header size is 3 bytes
			if(_data.GetLength() >= MPEGTS_TABLE_HEADER_SIZE)
			{
				BitReader parser(_data.GetWritableDataAs<uint8_t>(), MPEGTS_TABLE_HEADER_SIZE);
				if(ParseTableHeader(&parser) == false)
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
		if(consumed_length == length)
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
		if(collected_section_length < _section_length)
		{
			// not completed, need more data
			return consumed_length;
		}
		else if(collected_section_length == _section_length)
		{
			// completed
			BitReader parser(_data.GetWritableDataAs<uint8_t>(), _data.GetLength());
			parser.SkipBytes(MPEGTS_TABLE_HEADER_SIZE);
			ParseTableData(&parser);

			if(consumed_length < length)
			{
				// Check if remaining data is suffing bytes (0xFF) then skip all stuffing bytes
				uint8_t stuff = *(data + consumed_length);
				if(stuff == 0xFF)
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

	bool Section::ParseTableHeader(BitReader *parser)
	{
		if(_header_parsed == true)
		{
			// Already parsed
			return false;
		}

		if(parser->BytesReamined() < 3)
		{
			return false;
		}

		_table_id = parser->ReadBytes<uint8_t>();
		// s: section_syntax_indicator
		_section_syntax_indicator = parser->ReadBit();
		_private_bit = parser->ReadBit();
		_reserved_bits = parser->ReadBits<uint8_t>(2);
		if(_reserved_bits != 0x03)
		{
			// error
			return false;
		}
		// section_length: 12 bits
		_section_length_unused_bits = parser->ReadBits<uint16_t>(2);
		if(_section_length_unused_bits != 0)
		{
			// error
			return false;
		}
		_section_length = parser->ReadBits<uint16_t>(10);
		if(_section_length != parser->BytesReamined() && _section_length > 1021)
		{
			// error
			return false;
		}

		_header_parsed = true;

		return true;
	}

	bool Section::ParseTableData(BitReader *parser)
	{
		if(_header_parsed == false)
		{
			// header is not parsed
			return false;
		}

		// not enough data size to parse
		if(parser->BytesReamined() < MPEGTS_MIN_TABLE_DATA_SIZE)
		{
			return false;
		}

		_table_id_extension = parser->ReadBytes<uint16_t>();
		_reserved_bits2 = parser->ReadBits<uint8_t>(2);
		if(_reserved_bits2 != 0x03)
		{
			// error
		}
		_version_number = parser->ReadBits<uint8_t>(5);
		_current_next_indicator = parser->ReadBoolBit();
		_section_number = parser->ReadBytes<uint8_t>();
		_last_section_number = parser->ReadBytes<uint8_t>();

		switch(_table_id)
		{
			case static_cast<uint8_t>(WellKnownTableId::PROGRAM_ASSOCIATION_SECTION):
				if(ParsePat(parser) == false)
				{
					return false;
				}
				break;
			case static_cast<uint8_t>(WellKnownTableId::PROGRAM_MAP_SECTION):
				if(ParsePmt(parser) == false)
				{
					return false;
				}
				break;

			default:
				// Doesn't support
				break;
		}

		_crc = parser->ReadBytes<uint32_t>();

		/*TODO(Getroot): It seems having a problem, check later
		auto _check_crc = ov::CRC::Crc32(0, _data.GetWritableDataAs<uint8_t>(), MPEGTS_TABLE_HEADER_SIZE + _section_length - 4); // excluding last 4 bytes for crc
		if(_crc != _check_crc)
		{
			// error
			return false;
		}
		*/

		_completed = true;

		return true;
	}

	bool Section::ParsePat(BitReader *parser)
	{
		_pat = std::make_shared<PAT>();

		_pat->_program_num = parser->ReadBytes<uint16_t>();
		_pat->_reserved_bits = parser->ReadBits<uint8_t>(3);
		_pat->_program_map_pid = parser->ReadBits<uint16_t>(13);

		return true;
	}

	bool Section::ParsePmt(BitReader *parser)
	{
		_pmt = std::make_shared<PMT>();

		_pmt->_reserved_bits = parser->ReadBits<uint8_t>(3);
		_pmt->_pcr_pid = parser->ReadBits<uint16_t>(13);
		_pmt->_reserved_bits = parser->ReadBits<uint8_t>(4);
		_pmt->_program_info_length_unused_bits = parser->ReadBits<uint8_t>(2);
		_pmt->_program_info_length = parser->ReadBits<uint16_t>(10);

		// Descriptors
		auto descriptors_length = _pmt->_program_info_length;
		while(descriptors_length > 0)
		{
			auto descriptor = ParseDescriptor(parser);
			if(descriptor == nullptr)
			{
				// error
				return false;
			}

			_pmt->_program_descriptors.push_back(descriptor);

			descriptors_length -= (DESCRIPTOR_HEADER_SIZE + descriptor->_length);
		}

		// es info, remaining bytes excluding CRC(32bits)
		while(parser->BytesReamined() - 4 > 0)
		{
			auto es_info = std::make_shared<ESInfo>();

			es_info->_stream_type = parser->ReadBytes<uint8_t>();
			es_info->_reserved_bits = parser->ReadBits<uint8_t>(3);
			es_info->_elementary_pid = parser->ReadBits<uint16_t>(13);
			es_info->_reserved_bits2 = parser->ReadBits<uint8_t>(4);
			es_info->_es_info_length_unused_bits = parser->ReadBits<uint8_t>(2);
			es_info->_es_info_length = parser->ReadBits<uint16_t>(10);

			// Descriptors
			descriptors_length = es_info->_es_info_length;
			while(descriptors_length > 0)
			{
				auto descriptor = ParseDescriptor(parser);
				if(descriptor == nullptr)
				{
					// error
					return false;
				}

				es_info->_es_descriptors.push_back(descriptor);
				descriptors_length -= (DESCRIPTOR_HEADER_SIZE + descriptor->_length);
			}

			_pmt->_es_info_list.push_back(es_info);
		}

		return true;
	}

	std::shared_ptr<Descriptor> Section::ParseDescriptor(BitReader *parser)
	{
		auto descriptor = std::make_shared<Descriptor>();

		if(parser->BytesReamined() < 2)
		{
			return nullptr;
		}

		descriptor->_tag = parser->ReadBytes<uint8_t>();
		descriptor->_length = parser->ReadBytes<uint8_t>();

		if(parser->BytesReamined() < descriptor->_length)
		{
			return nullptr;
		}

		descriptor->_data = parser->CurrentPosition();
		parser->SkipBytes(descriptor->_length);

		return descriptor;
	}

	// return true when section is completed
	bool Section::IsCompleted()
	{
		return _completed;
	}

	// Getter
	uint16_t Section::PID()
	{
		return _pid;
	}

	// Section header
	uint8_t	Section::TableId()
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

	uint16_t Section::TableIdExtension()
	{
		return _table_id_extension;
	}

	uint8_t Section::VersionNumber()
	{
		return _version_number;
	}

	bool Section::CurrentNextIndicator()
	{
		return _current_next_indicator;
	}

	uint8_t Section::SectionNumber()
	{
		return _section_number;
	}

	uint8_t Section::LastSectionNumber()
	{
		return _last_section_number;
	}
}