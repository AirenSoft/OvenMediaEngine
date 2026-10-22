//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2026 OvenMediaLabs. All rights reserved.
//
//==============================================================================
#include "mpegts_depacketizer.h"

#include <base/ovlibrary/crc.h>
#include <gtest/gtest.h>

#include <vector>

namespace
{
	constexpr size_t TS_PACKET_SIZE		   = 188;
	constexpr size_t TS_HEADER_SIZE		   = 4;

	// A complete PAT section for program 1 whose PMT is on PID 0x1000.
	const std::vector<uint8_t> PAT_SECTION = {
		// table_id=0, section_syntax_indicator=1, section_length=13
		0x00, 0xB0, 0x0D,
		// transport_stream_id=1, version=0, current_next=1, section_number=0, last_section_number=0
		0x00, 0x01, 0xC1, 0x00, 0x00,
		// program_number=1, program_map_PID=0x1000
		0x00, 0x01, 0xF0, 0x00,
		// CRC_32
		0x2A, 0xB1, 0x04, 0xB2};

	// A TS packet whose payload is exactly `payload`, padded in front by an adaptation field of stuffing bytes.
	std::vector<uint8_t> MakePacket(uint16_t pid, bool pusi, uint8_t cc, const std::vector<uint8_t> &payload)
	{
		std::vector<uint8_t> packet(TS_PACKET_SIZE, 0xFF);

		const size_t adaptation_size = TS_PACKET_SIZE - TS_HEADER_SIZE - payload.size();

		packet[0]					 = 0x47;
		packet[1]					 = static_cast<uint8_t>((pusi ? 0x40 : 0x00) | ((pid >> 8) & 0x1F));
		packet[2]					 = static_cast<uint8_t>(pid & 0xFF);
		packet[3]					 = static_cast<uint8_t>(((adaptation_size > 0) ? 0x30 : 0x10) | (cc & 0x0F));

		if (adaptation_size > 0)
		{
			// adaptation_field_length, then a flags byte with no flag set; the rest is stuffing
			packet[4] = static_cast<uint8_t>(adaptation_size - 1);
			if (adaptation_size > 1)
			{
				packet[5] = 0x00;
			}
		}

		std::copy(payload.begin(), payload.end(), packet.begin() + TS_HEADER_SIZE + adaptation_size);

		return packet;
	}

	// A PMT section for program 1 with one H.264 ES on PID 0x0100, whose ES_info_length is `es_info_length`
	// though no descriptor byte follows, so a nonzero value makes the section unparsable.
	std::vector<uint8_t> MakePmtSection(uint16_t es_info_length)
	{
		std::vector<uint8_t> bytes = {
			// table_id=2, section_syntax_indicator=1, section_length=18
			0x02, 0xB0, 0x12,
			// program_number=1, version=0, current_next=1, section_number=0, last_section_number=0
			0x00, 0x01, 0xC1, 0x00, 0x00,
			// PCR_PID=0x0100, program_info_length=0
			0xE1, 0x00, 0xF0, 0x00,
			// stream_type=H.264, elementary_PID=0x0100, ES_info_length
			0x1B, 0xE1, 0x00, static_cast<uint8_t>(0xF0 | ((es_info_length >> 8) & 0x03)), static_cast<uint8_t>(es_info_length & 0xFF)};

		const auto crc = ov::CRC::Crc32Mpeg2(bytes.data(), bytes.size());
		bytes.insert(bytes.end(), {static_cast<uint8_t>(crc >> 24), static_cast<uint8_t>(crc >> 16), static_cast<uint8_t>(crc >> 8), static_cast<uint8_t>(crc)});

		return bytes;
	}

	std::vector<uint8_t> WithPointerField(uint8_t pointer_field, const std::vector<uint8_t> &section)
	{
		std::vector<uint8_t> payload = {pointer_field};
		payload.insert(payload.end(), section.begin(), section.end());
		return payload;
	}

	size_t GetPmtCount(mpegts::MpegTsDepacketizer &depacketizer)
	{
		std::vector<std::shared_ptr<mpegts::Section>> pmt_list;
		depacketizer.GetPMTList(1, &pmt_list);
		return pmt_list.size();
	}

	std::shared_ptr<ov::Data> ToData(const std::vector<std::vector<uint8_t>> &packets)
	{
		auto data = std::make_shared<ov::Data>();

		for (const auto &packet : packets)
		{
			data->Append(packet.data(), packet.size());
		}

		return data;
	}
}  // namespace

// A PUSI packet whose pointer_field claims more bytes than its payload holds must not complete the pending
// section with bytes from beyond the payload (here, the next packet of the same datagram).
TEST(MpegTsDepacketizerTest, PointerFieldBeyondPayloadDoesNotCompletePendingSection)
{
	mpegts::MpegTsDepacketizer depacketizer;

	// pointer_field=0, then the first 4 bytes of the PAT section, leaving 12 bytes pending
	std::vector<uint8_t> first_payload = {0x00};
	first_payload.insert(first_payload.end(), PAT_SECTION.begin(), PAT_SECTION.begin() + 4);

	// pointer_field=12 with no byte after it
	const std::vector<uint8_t> second_payload = {0x0C};

	// The next 12 bytes of the datagram complete a valid PAT (transport_stream_id 0x0047, program 1 on PID 0x1000).
	// They start with a sync byte, and the transport_error_indicator in the next byte makes that packet itself dropped.
	std::vector<uint8_t> pending			  = {PAT_SECTION[0], PAT_SECTION[1], PAT_SECTION[2], PAT_SECTION[3],
												 0x47, 0xC1, 0x00, 0x00, 0x00, 0x01, 0xF0, 0x00};
	const auto crc							  = ov::CRC::Crc32Mpeg2(pending.data(), pending.size());
	std::vector<uint8_t> next_packet(TS_PACKET_SIZE, 0xFF);
	std::copy(pending.begin() + 4, pending.end(), next_packet.begin());
	next_packet[8]	= static_cast<uint8_t>(crc >> 24);
	next_packet[9]	= static_cast<uint8_t>(crc >> 16);
	next_packet[10] = static_cast<uint8_t>(crc >> 8);
	next_packet[11] = static_cast<uint8_t>(crc);

	depacketizer.AddPacket(ToData({MakePacket(0x0000, true, 0, first_payload),
								   MakePacket(0x0000, true, 1, second_payload),
								   next_packet}));

	EXPECT_EQ(depacketizer.GetFirstPAT(), nullptr);
}

// A PUSI packet whose pointer_field runs past its payload must not have the same bytes parsed as a new section.
TEST(MpegTsDepacketizerTest, PointerFieldBeyondPayloadDropsThePacket)
{
	mpegts::MpegTsDepacketizer depacketizer;

	// pointer_field=200, though only the 16 bytes of the PAT section follow it
	std::vector<uint8_t> payload = {200};
	payload.insert(payload.end(), PAT_SECTION.begin(), PAT_SECTION.end());

	depacketizer.AddPacket(ToData({MakePacket(0x0000, true, 0, payload)}));

	EXPECT_EQ(depacketizer.GetFirstPAT(), nullptr);
}

// A well-formed PAT that starts after the tail of the previous section is still parsed.
TEST(MpegTsDepacketizerTest, PointerFieldWithinPayloadParsesTheNewSection)
{
	mpegts::MpegTsDepacketizer depacketizer;

	// pointer_field=3, three bytes of a previous section tail, then the PAT section
	std::vector<uint8_t> payload = {0x03, 0xAA, 0xAA, 0xAA};
	payload.insert(payload.end(), PAT_SECTION.begin(), PAT_SECTION.end());

	depacketizer.AddPacket(ToData({MakePacket(0x0000, true, 0, payload)}));

	auto pat = depacketizer.GetFirstPAT();
	ASSERT_NE(pat, nullptr);
	EXPECT_EQ(pat->_program_num, 1);
	EXPECT_EQ(pat->_program_map_pid, 0x1000);
}

// A section that arrived whole but could not be parsed is dropped, so a later section on the same PID that spans
// several packets is still assembled.
TEST(MpegTsDepacketizerTest, UnparsableSectionDoesNotBlockTheNextSection)
{
	mpegts::MpegTsDepacketizer depacketizer;

	const auto valid_pmt				  = MakePmtSection(0);

	// The valid PMT split across two packets
	const std::vector<uint8_t> first_part = WithPointerField(0x00, std::vector<uint8_t>(valid_pmt.begin(), valid_pmt.begin() + 10));
	const std::vector<uint8_t> second_part(valid_pmt.begin() + 10, valid_pmt.end());

	depacketizer.AddPacket(ToData({MakePacket(0x0000, true, 0, WithPointerField(0x00, PAT_SECTION)),
								   MakePacket(0x1000, true, 0, WithPointerField(0x00, MakePmtSection(8))),
								   MakePacket(0x1000, true, 1, first_part),
								   MakePacket(0x1000, false, 2, second_part)}));

	EXPECT_EQ(GetPmtCount(depacketizer), 1u);
}

// A PUSI packet ends the pending section on its PID even when the pending section cannot be completed.
TEST(MpegTsDepacketizerTest, IncompleteSectionIsDiscardedAtTheNextSectionStart)
{
	mpegts::MpegTsDepacketizer depacketizer;

	const auto valid_pmt				  = MakePmtSection(0);

	const std::vector<uint8_t> first_part = WithPointerField(0x00, std::vector<uint8_t>(valid_pmt.begin(), valid_pmt.begin() + 10));
	const std::vector<uint8_t> second_part(valid_pmt.begin() + 10, valid_pmt.end());

	// The start of a 30-byte section that never gets the rest, then the PMT starting with pointer_field 0
	const std::vector<uint8_t> stray_start = WithPointerField(0x00, {0x02, 0xB0, 0x1E, 0x00, 0x01, 0xC1, 0x00, 0x00});

	depacketizer.AddPacket(ToData({MakePacket(0x0000, true, 0, WithPointerField(0x00, PAT_SECTION)),
								   MakePacket(0x1000, true, 0, stray_start),
								   MakePacket(0x1000, true, 1, first_part),
								   MakePacket(0x1000, false, 2, second_part)}));

	EXPECT_EQ(GetPmtCount(depacketizer), 1u);
}

// A table the depacketizer does not handle does not stop the sections after it in the same packet.
TEST(MpegTsDepacketizerTest, UnsupportedTableDoesNotStopTheFollowingSection)
{
	mpegts::MpegTsDepacketizer depacketizer;

	// A section with an unhandled table_id 0xC0, section_length 5 and a valid CRC_32, then the PAT
	std::vector<uint8_t> payload = WithPointerField(0x00, {0xC0, 0xB0, 0x05, 0x00, 0xED, 0x05, 0x8B, 0x56});
	payload.insert(payload.end(), PAT_SECTION.begin(), PAT_SECTION.end());

	depacketizer.AddPacket(ToData({MakePacket(0x0000, true, 0, payload)}));

	EXPECT_NE(depacketizer.GetFirstPAT(), nullptr);
}
