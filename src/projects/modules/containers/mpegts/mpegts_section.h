//==============================================================================
//
//  MPEGTS Section
//
//  Created by Getroot
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include <vector>
#include <memory>
#include <base/ovlibrary/ovlibrary.h>
#include <base/ovlibrary/bit_reader.h>


namespace mpegts
{	
	constexpr int MPEGTS_TABLE_HEADER_SIZE = 3;
	constexpr int MPEGTS_MIN_TABLE_DATA_SIZE = 9;

	struct PAT
	{
		uint16_t	_program_num; // 16bits
		uint8_t		_reserved_bits = 0x07; // 3bits (0x07)
		uint16_t	_program_map_pid; // associated PMT
	};

	#define DESCRIPTOR_HEADER_SIZE	2

	enum class WellKnownDescriptorTags : uint8_t
	{
		H264_VIDEO_STREAM_HEADER_PARAMS = 0x28,
		ADTS_AAC_AUDIO_STREAM_HEADER_PARAMS = 0x2B
	};

	struct Descriptor
	{
		uint8_t		_tag; // 8bits
		uint8_t 	_length = 0; // 8bits
		const uint8_t*	_data = nullptr; // _length
	};

	enum class WellKnownStreamTypes : uint8_t
	{
		None = 0x00,
		H264 = 0x1B, // 27
		H265 = 0x24, // 36
		AAC = 0x0F, // 15 AAC ADTS
		AAC_LATM = 0x11 // 17 AAC LATM
	};

	struct ESInfo
	{
		uint8_t		_stream_type; // 8bits, WellKnownStreamTypes are supported now
		uint8_t		_reserved_bits = 0x07; // 3bits
		uint16_t	_elementary_pid; // 13 bits
		uint8_t		_reserved_bits2 = 0x0F; // 4bits
		uint8_t		_es_info_length_unused_bits = 0; // 2bits
		uint8_t		_es_info_length = 0; // 10bits
		std::vector<std::shared_ptr<Descriptor>> _es_descriptors;
	};

	struct PMT
	{
		uint16_t	_pid = 0; // from MPEGTS Header

		uint8_t		_reserved_bits = 0x07; // 3bits
		uint16_t	_pcr_pid = 0X1FFF; // 13bits, program clock reference, if this is unused then it is set to 0x1FFF
		uint8_t		_reserved_bits2 = 0x0F; // 4bits 
		uint8_t		_program_info_length_unused_bits = 0; // 2bits
		uint16_t	_program_info_length = 0; //10bits, the number of bytes that follow for the program descriptors
		std::vector<std::shared_ptr<Descriptor>> _program_descriptors;
		std::vector<std::shared_ptr<ESInfo>> _es_info_list;
	};

	enum class WellKnownTableId : uint8_t
	{
		PROGRAM_ASSOCIATION_SECTION = 0x00,
		CONDITIONAL_ACCESS_SECTION = 0x01,
		PROGRAM_MAP_SECTION = 0x02,
		TRANSPORT_STREAM_DESCRIPTION_SECTION = 0x03,
		SCENE_DESCRIPTION_SECTION = 0x04,
		OBJECT_DESCRIPTION_SECTION = 0x05,
		METADATA_SECTION = 0x06,
		IPMP_CONTROL_INFORMATION = 0x07,
		NULL_PADDING = 0xFF	
	};

	/*
		Packet 1: [TS Header][P][Section                                    ] : uint_starting_indicator = 1
		Packet 2: [TS Header][                    Section                   ]
		Packet 3: [TS Header][P][stuffing                  ][    Section    ] : Section length == received section length
		(Assemble packet 1,2,3)
	*/

	class Section
	{
	public:
		Section();
		Section(uint16_t pid);
		~Section();
		
		// Build section from PAT, PMT. 
		// Now it only supports sizes that can fit in one section.
		static std::shared_ptr<Section> Build(const PAT &pat);
		static std::shared_ptr<Section> Build(const PMT &pmt);

		// return consumed length (including stuff)
		size_t AppendData(const uint8_t *data, uint32_t length);
		// return true when section is completed
		bool IsCompleted();

		// Getter
		uint16_t PID();
		// Section header
		uint8_t	TableId();
		bool	SectionSyntaxIndicator();
		uint16_t SectionLength();
		uint16_t TableIdExtension();
		uint8_t VersionNumber();
		bool CurrentNextIndicator();
		uint8_t SectionNumber();
		uint8_t LastSectionNumber();

		std::shared_ptr<PAT> GetPAT();
		std::shared_ptr<PMT> GetPMT();

		// Get Data
		const ov::Data &GetData() {
			return _data;
		}

	private:
		bool ParseTableHeader(BitReader *parser);
		bool ParseTableData(BitReader *parser);
		bool ParsePat(BitReader *parser);
		bool ParsePmt(BitReader *parser);
		std::shared_ptr<Descriptor> ParseDescriptor(BitReader *parser);

		bool _header_parsed = false;
		bool _completed = false;

		uint16_t _pid; // from MPEGTS Header

		// Table header
		uint8_t	_table_id = 0; // 8bits, 
		uint8_t	_section_syntax_indicator = 1; // 1bit (PAT/PMT/CAT:1)
		uint8_t	_private_bit = 0; // 1bit (PAT/PMT/CAT:0 Others:1)
		uint8_t	_reserved_bits = 0x03; // 2bits
		uint8_t	_section_length_unused_bits = 0; // 2bits
		uint16_t _section_length = 0; // 10bits, <=1021

		// Table data
		uint16_t _table_id_extension = 0U;		// 16 bits (PAT: Transport stream id, PMT: Program number)
		uint8_t _reserved_bits2 = 0x03;			// 2 bits, 0x03
		uint8_t _version_number = 0U;		  	// 5 bits
		bool _current_next_indicator = false;  	// 1 bit
		uint8_t _section_number = 0U;		  	// 8 bits, starts from 0
		uint8_t _last_section_number = 0U;	 	// 8 bits

		ov::Data _data; // All section data
		uint32_t _crc; // checksum, excluding the pointer field

		// One table per section
		std::shared_ptr<PAT>		_pat = nullptr;
		std::shared_ptr<PMT>		_pmt = nullptr;
	};
}