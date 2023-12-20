//==============================================================================
//
//  MPEGTS PES
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

#define MPEGTS_PES_HEADER_SIZE					6U
#define MPEGTS_MIN_PES_OPTIONAL_HEADER_SIZE		3U

namespace mpegts
{
	enum class WellKnownStreamId : uint8_t
	{
		PROGRAM_STREAM_MAP 		= 0b10111100,
		PRIVATE_STREAM_1		= 0b10111101,
		PADDING_STREAM 			= 0b10111110,
		PRIVATE_STREAM_2 		= 0b10111111,

		// OME supports only 110xxxxx, 1110xxxx, it has pes optional header
		// 110x xxxx	ISO/IEC 13818-3 or ISO/IEC 11172-3 or ISO/IEC 13818-7 or ISO/IEC 14496-3 audio stream number xxxxx : has pes optional fields (0xC0 ~ 0xDF)
		// 1110 xxxx	ITU-T Rec. H.262 | ISO/IEC 13818-2 or ISO/IEC 11172-2 or ISO/IEC 14496-2 video stream number xxxx : has pes optional fields (0xE0 ~ 0xEF)
		
		ECM_STREAM 				= 0b11110000,
		EMM_STREAM 				= 0b11110001,
		DSMCC 					= 0b11110010,
		ISO_IEC_13522_STREAM 	= 0b11110011,
		H222_1_TYPE_A			= 0b11110100,
		H222_1_TYPE_B			= 0b11110101,
		H222_1_TYPE_C			= 0b11110110,
		H222_1_TYPE_D			= 0b11110111,
		H222_1_TYPE_E			= 0b11111000,
		PROGRAM_STREAM_DIRECTORY = 0b11111111
	};

	class Pes
	{
	public:
		Pes(uint16_t pid);
		~Pes();
		
		// return consumed length
		size_t AppendData(const uint8_t *data, uint32_t length);
		// All pes data has been inserted
		bool SetEndOfData();

		// return true when section is completed when PES packet length is not zero
		bool IsCompleted();

		// Getter

		// Header
		uint16_t PID();
		uint8_t StreamId();
		uint16_t PesPacketLength();

		// Optional PES Header ( >= 3bytes )
		uint8_t ScramblingControl();
		uint8_t Priority();
		uint8_t DataAlignmentIndicator();
		uint8_t Copyright();
		uint8_t OriginalOrCopy();
		int64_t Pts();
		int64_t Dts();

		const uint8_t* Payload();
		uint32_t PayloadLength();

		inline bool IsAudioStream() const
		{
			// Audio stream: 0b110xxxxx
			return (_stream_id & 0b11100000) == 0b11000000;
		}

		inline bool IsVideoStream() const
		{
			// Video stream: 0b1110xxxx
			return (_stream_id & 0b11110000) == 0b11100000;
		}

	private:
		bool HasOptionalHeader();
		bool HasOptionalData();
		bool ParsePesHeader(BitReader *parser);
		bool ParsePesOptionalHeader(BitReader *parser);
		bool ParsePesOPtionalData(BitReader *parser);
		bool ParseTimestamp(BitReader *parser, uint8_t start_bits, int64_t &timestamp);

		bool _pes_header_parsed = false;
		bool _pes_optional_header_parsed = false;
		bool _pes_optional_data_parsed = false;

		bool _completed = false;

		uint16_t _pid; // from MPEGTS Header

		// PES Header
		uint8_t _start_code_prefix_1 = 0x00;	// 8 bit (0x00)
		uint8_t _start_code_prefix_2 = 0x00;	// 8 bit (0x00)
		uint8_t _start_code_prefix_3 = 0x01;	// 8 bit (0x01)

		uint8_t _stream_id = 0U;				// 8 bits
		uint16_t _pes_packet_length = 0U;		// 16 bits

		// Optional PES Header ( >= 3bytes )
		uint8_t _marker_bits = 0U;				// 2 bits (10)
		uint8_t _scrambling_control = 0U;		// 2 bits (00 : not scrambled)
		uint8_t _priority = 0U;					// 1 bit
		uint8_t _data_alignment_indicator = 0U; // 1 bit (1 : video start code or audio syncword)
		uint8_t _copyright = 0U;				// 1 bit
		uint8_t _original_or_copy = 0U;			// 1 bit
		uint8_t _pts_dts_flags = 0U;			// 2 bits (11: both, 10: only pts, 01: forbidden, 00: no)
		uint8_t _escr_flag = 0U;				// 1 bit 
		uint8_t _es_rate_flag = 0U;				// 1 bit
		uint8_t _dsm_trick_mode_flag = 0U;		// 1 bit
		uint8_t _additional_copy_info_flag = 0U;// 1 bit
		uint8_t _crc_flag = 0U;					// 1 bit
		uint8_t _extension_flag = 0U;			// 1 bit
		uint8_t _header_data_length = 0U;		// 8 bits

		int64_t _pts = -1LL;
		int64_t _dts = -1LL;

		ov::Data _data;
		uint8_t* _payload = nullptr;
		uint32_t _payload_length = 0;
	};
}