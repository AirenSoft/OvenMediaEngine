//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2026 OvenMediaLabs. All rights reserved.
//
//==============================================================================
#include "mpegts_section.h"

#include <base/ovlibrary/crc.h>
#include <gtest/gtest.h>

#include <utility>
#include <vector>

namespace
{
	constexpr uint16_t PMT_PID = 0x1000;

	// Appends the `CRC_32` of `bytes` to it.
	void AppendCrc(std::vector<uint8_t> &bytes)
	{
		const auto crc = ov::CRC::Crc32Mpeg2(bytes.data(), bytes.size());

		bytes.insert(bytes.end(), {static_cast<uint8_t>(crc >> 24), static_cast<uint8_t>(crc >> 16), static_cast<uint8_t>(crc >> 8), static_cast<uint8_t>(crc)});
	}

	// A PMT section for program 1 with one H.264 ES on PID 0x0100, then `extra_bytes` zero bytes and the CRC field.
	std::vector<uint8_t> MakePmtSectionBytes(size_t extra_bytes, uint16_t es_info_length = 0)
	{
		const auto section_length  = 9 + 5 + extra_bytes + 4;

		std::vector<uint8_t> bytes = {
			// table_id, section_syntax_indicator=1, private=0, reserved=11, section_length
			0x02,
			static_cast<uint8_t>(0xB0 | ((section_length >> 8) & 0x03)),
			static_cast<uint8_t>(section_length & 0xFF),
			// program_number=1, version=0, current_next=1, section_number=0, last_section_number=0
			0x00, 0x01, 0xC1, 0x00, 0x00,
			// PCR_PID=0x0100, program_info_length=0
			0xE1, 0x00, 0xF0, 0x00,
			// stream_type=H.264, elementary_PID=0x0100, ES_info_length
			0x1B, 0xE1, 0x00,
			static_cast<uint8_t>(0xF0 | ((es_info_length >> 8) & 0x03)),
			static_cast<uint8_t>(es_info_length & 0xFF)};

		bytes.insert(bytes.end(), extra_bytes, 0x00);
		AppendCrc(bytes);

		return bytes;
	}

	// A PMT section for program 1 with one H.264 ES on PID 0x0100, whose program and ES descriptor loops
	// declare the given lengths and hold the given bytes, then the CRC field.
	std::vector<uint8_t> MakePmtSectionBytesWithDescriptors(uint16_t program_info_length, const std::vector<uint8_t> &program_info,
															uint16_t es_info_length, const std::vector<uint8_t> &es_info)
	{
		const auto section_length  = 9 + program_info.size() + 5 + es_info.size() + 4;

		std::vector<uint8_t> bytes = {
			// table_id, section_syntax_indicator=1, private=0, reserved=11, section_length
			0x02,
			static_cast<uint8_t>(0xB0 | ((section_length >> 8) & 0x03)),
			static_cast<uint8_t>(section_length & 0xFF),
			// program_number=1, version=0, current_next=1, section_number=0, last_section_number=0
			0x00, 0x01, 0xC1, 0x00, 0x00,
			// PCR_PID=0x0100, program_info_length
			0xE1, 0x00,
			static_cast<uint8_t>(0xF0 | ((program_info_length >> 8) & 0x03)),
			static_cast<uint8_t>(program_info_length & 0xFF)};

		bytes.insert(bytes.end(), program_info.begin(), program_info.end());

		// stream_type=H.264, elementary_PID=0x0100, ES_info_length
		bytes.insert(bytes.end(), {0x1B, 0xE1, 0x00, static_cast<uint8_t>(0xF0 | ((es_info_length >> 8) & 0x03)), static_cast<uint8_t>(es_info_length & 0xFF)});
		bytes.insert(bytes.end(), es_info.begin(), es_info.end());
		AppendCrc(bytes);

		return bytes;
	}

	// registration_descriptor ("HDMV") and ISO_639_language_descriptor ("eng"), 6 bytes each.
	const std::vector<uint8_t> REGISTRATION_DESCRIPTOR = {0x05, 0x04, 'H', 'D', 'M', 'V'};
	const std::vector<uint8_t> LANGUAGE_DESCRIPTOR	   = {0x0A, 0x04, 'e', 'n', 'g', 0x00};
}  // namespace

TEST(MpegTsSectionTest, ParsesWellFormedPmt)
{
	mpegts::PMT pmt;
	pmt._table_id_extension		= 1;
	pmt._current_next_indicator = true;
	pmt._pcr_pid				= 0x0100;

	const auto video			= std::make_shared<mpegts::ESInfo>();
	video->_stream_type			= static_cast<uint8_t>(mpegts::WellKnownStreamTypes::H264);
	video->_elementary_pid		= 0x0100;
	pmt._es_info_list.push_back(video);

	const uint8_t language[] = {'e', 'n', 'g', 0x00};
	const auto audio		 = std::make_shared<mpegts::ESInfo>();
	audio->_stream_type		 = static_cast<uint8_t>(mpegts::WellKnownStreamTypes::AAC);
	audio->_elementary_pid	 = 0x0101;
	audio->_es_descriptors.push_back(std::make_shared<mpegts::Descriptor>(0x0A, std::make_shared<ov::Data>(language, sizeof(language))));
	audio->_es_info_length = audio->_es_descriptors.front()->GetPacketLength();
	pmt._es_info_list.push_back(audio);

	const auto built = mpegts::Section::Build(PMT_PID, pmt);
	const auto &data = built->GetData();

	mpegts::Section section(PMT_PID);
	ASSERT_EQ(section.AppendData(data.GetDataAs<uint8_t>(), data.GetLength()), data.GetLength());
	ASSERT_TRUE(section.IsCompleted());

	const auto &es_info_list = section.GetPMT()->_es_info_list;
	ASSERT_EQ(es_info_list.size(), 2u);
	EXPECT_EQ(es_info_list[0]->_stream_type, static_cast<uint8_t>(mpegts::WellKnownStreamTypes::H264));
	EXPECT_EQ(es_info_list[0]->_elementary_pid, 0x0100);
	EXPECT_TRUE(es_info_list[0]->_es_descriptors.empty());
	EXPECT_EQ(es_info_list[1]->_stream_type, static_cast<uint8_t>(mpegts::WellKnownStreamTypes::AAC));
	EXPECT_EQ(es_info_list[1]->_elementary_pid, 0x0101);
	ASSERT_EQ(es_info_list[1]->_es_descriptors.size(), 1u);
	EXPECT_EQ(es_info_list[1]->_es_descriptors[0]->GetTag(), 0x0A);
}

TEST(MpegTsSectionTest, ParsesPmtWithNoBytesAfterLastEs)
{
	const auto bytes = MakePmtSectionBytes(0);

	mpegts::Section section(PMT_PID);
	ASSERT_EQ(section.AppendData(bytes.data(), bytes.size()), bytes.size());
	ASSERT_TRUE(section.IsCompleted());

	const auto &es_info_list = section.GetPMT()->_es_info_list;
	ASSERT_EQ(es_info_list.size(), 1u);
	EXPECT_EQ(es_info_list[0]->_elementary_pid, 0x0100);
}

TEST(MpegTsSectionTest, RejectsPmtWithTruncatedEsEntry)
{
	for (size_t extra_bytes = 1; extra_bytes <= 4; extra_bytes++)
	{
		SCOPED_TRACE(extra_bytes);

		const auto bytes = MakePmtSectionBytes(extra_bytes);

		mpegts::Section section(PMT_PID);
		EXPECT_EQ(section.AppendData(bytes.data(), bytes.size()), bytes.size());
		EXPECT_FALSE(section.IsCompleted());
	}
}

TEST(MpegTsSectionTest, RejectsPmtWithEsInfoLengthOverrun)
{
	// 0x100 sets no bit of the low byte.
	for (const uint16_t es_info_length : {0x008, 0x100})
	{
		SCOPED_TRACE(es_info_length);

		const auto bytes = MakePmtSectionBytes(0, es_info_length);

		mpegts::Section section(PMT_PID);
		EXPECT_EQ(section.AppendData(bytes.data(), bytes.size()), bytes.size());
		EXPECT_FALSE(section.IsCompleted());
	}
}

TEST(MpegTsSectionTest, ParsesPmtWithDescriptors)
{
	const auto bytes = MakePmtSectionBytesWithDescriptors(6, REGISTRATION_DESCRIPTOR, 6, LANGUAGE_DESCRIPTOR);

	mpegts::Section section(PMT_PID);
	ASSERT_EQ(section.AppendData(bytes.data(), bytes.size()), bytes.size());
	ASSERT_TRUE(section.IsCompleted());

	const auto pmt = section.GetPMT();
	ASSERT_EQ(pmt->_program_descriptors.size(), 1u);
	EXPECT_EQ(pmt->_program_descriptors[0]->GetTag(), 0x05);
	ASSERT_EQ(pmt->_es_info_list.size(), 1u);
	ASSERT_EQ(pmt->_es_info_list[0]->_es_descriptors.size(), 1u);
	EXPECT_EQ(pmt->_es_info_list[0]->_es_descriptors[0]->GetTag(), 0x0A);
}

TEST(MpegTsSectionTest, RejectsPmtWithProgramDescriptorOverrun)
{
	// A 6-byte descriptor in a 4-byte loop, and a loop too short for a descriptor header.
	for (const auto &[program_info_length, program_info] : std::vector<std::pair<uint16_t, std::vector<uint8_t>>>{
			 {4, REGISTRATION_DESCRIPTOR},
			 {1, {0x05}}})
	{
		SCOPED_TRACE(program_info_length);

		const auto bytes = MakePmtSectionBytesWithDescriptors(program_info_length, program_info, 0, {});

		mpegts::Section section(PMT_PID);
		EXPECT_EQ(section.AppendData(bytes.data(), bytes.size()), bytes.size());
		EXPECT_FALSE(section.IsCompleted());
	}
}

TEST(MpegTsSectionTest, RejectsPmtWithEsDescriptorOverrun)
{
	// A 6-byte descriptor in a 4-byte loop.
	const auto bytes = MakePmtSectionBytesWithDescriptors(0, {}, 4, LANGUAGE_DESCRIPTOR);

	mpegts::Section section(PMT_PID);
	EXPECT_EQ(section.AppendData(bytes.data(), bytes.size()), bytes.size());
	EXPECT_FALSE(section.IsCompleted());
}

TEST(MpegTsSectionTest, RejectsPmtWithProgramInfoLengthOverrun)
{
	for (const uint16_t program_info_length : {0x008, 0x100})
	{
		SCOPED_TRACE(program_info_length);

		const auto bytes = MakePmtSectionBytesWithDescriptors(program_info_length, {}, 0, {});

		mpegts::Section section(PMT_PID);
		EXPECT_EQ(section.AppendData(bytes.data(), bytes.size()), bytes.size());
		EXPECT_FALSE(section.IsCompleted());
	}
}

namespace
{
	// A PAT section with the given (`program_number`, PID) entries and `extra_bytes` zero bytes, then the CRC field.
	std::vector<uint8_t> MakePatSectionBytes(const std::vector<std::pair<uint16_t, uint16_t>> &programs, size_t extra_bytes = 0)
	{
		const auto section_length  = 5 + (programs.size() * 4) + extra_bytes + 4;

		std::vector<uint8_t> bytes = {
			// `table_id`=0, `section_syntax_indicator`=1, private=0, reserved=11, `section_length`
			0x00,
			static_cast<uint8_t>(0xB0 | ((section_length >> 8) & 0x03)),
			static_cast<uint8_t>(section_length & 0xFF),
			// `transport_stream_id`=1, version=0, current_next=1, `section_number`=0, `last_section_number`=0
			0x00, 0x01, 0xC1, 0x00, 0x00};

		for (const auto &[program_number, pid] : programs)
		{
			bytes.insert(bytes.end(), {static_cast<uint8_t>(program_number >> 8), static_cast<uint8_t>(program_number & 0xFF),
									   static_cast<uint8_t>(0xE0 | ((pid >> 8) & 0x1F)), static_cast<uint8_t>(pid & 0xFF)});
		}

		bytes.insert(bytes.end(), extra_bytes, 0x00);
		AppendCrc(bytes);

		return bytes;
	}

	std::shared_ptr<mpegts::PAT> ParsePat(const std::vector<uint8_t> &bytes)
	{
		mpegts::Section section(0x0000);
		if ((section.AppendData(bytes.data(), bytes.size()) != bytes.size()) || (section.IsCompleted() == false))
		{
			return nullptr;
		}

		return section.GetPAT();
	}
}  // namespace

TEST(MpegTsSectionTest, PatSkipsNetworkInformationEntry)
{
	const auto pat = ParsePat(MakePatSectionBytes({{0, 0x0010}, {1, 0x1000}}));

	ASSERT_NE(pat, nullptr);
	EXPECT_EQ(pat->_program_num, 1);
	EXPECT_EQ(pat->_program_map_pid, 0x1000);
}

TEST(MpegTsSectionTest, PatUsesTheFirstOfSeveralPrograms)
{
	const auto pat = ParsePat(MakePatSectionBytes({{2, 0x1002}, {1, 0x1001}}));

	ASSERT_NE(pat, nullptr);
	EXPECT_EQ(pat->_program_num, 2);
	EXPECT_EQ(pat->_program_map_pid, 0x1002);
}

TEST(MpegTsSectionTest, RejectsPatWithoutProgram)
{
	EXPECT_EQ(ParsePat(MakePatSectionBytes({{0, 0x0010}})), nullptr);
	EXPECT_EQ(ParsePat(MakePatSectionBytes({})), nullptr);
}

TEST(MpegTsSectionTest, RejectsPatWithTruncatedEntry)
{
	EXPECT_EQ(ParsePat(MakePatSectionBytes({{1, 0x1000}}, 2)), nullptr);
}

// A rejected PAT leaves no half-filled table behind.
TEST(MpegTsSectionTest, RejectedPatHasNoTable)
{
	for (const auto &bytes : {MakePatSectionBytes({{0, 0x0010}}), MakePatSectionBytes({{1, 0x1000}}, 2)})
	{
		SCOPED_TRACE(bytes.size());

		mpegts::Section section(0x0000);
		EXPECT_EQ(section.AppendData(bytes.data(), bytes.size()), bytes.size());
		EXPECT_TRUE(section.IsFailed());
		EXPECT_EQ(section.GetPAT(), nullptr);
	}
}

// A rejected PMT leaves no half-filled table behind, on every failure path after the table is created.
TEST(MpegTsSectionTest, RejectedPmtHasNoTable)
{
	const std::vector<std::vector<uint8_t>> rejected = {
		// `program_info_length` past the section
		MakePmtSectionBytesWithDescriptors(0x008, {}, 0, {}),
		// A program descriptor past its loop
		MakePmtSectionBytesWithDescriptors(4, REGISTRATION_DESCRIPTOR, 0, {}),
		// An ES entry shorter than 5 bytes
		MakePmtSectionBytes(2),
		// `ES_info_length` past the section
		MakePmtSectionBytes(0, 0x008),
		// An ES descriptor past its loop
		MakePmtSectionBytesWithDescriptors(0, {}, 4, LANGUAGE_DESCRIPTOR)};

	for (size_t i = 0; i < rejected.size(); i++)
	{
		SCOPED_TRACE(i);

		const auto &bytes = rejected[i];

		mpegts::Section section(PMT_PID);
		EXPECT_EQ(section.AppendData(bytes.data(), bytes.size()), bytes.size());
		EXPECT_TRUE(section.IsFailed());
		EXPECT_EQ(section.GetPMT(), nullptr);
	}
}

TEST(MpegTsSectionTest, AcceptsPatWithEncoderCrc)
{
	// A PAT written by FFmpeg, whose `CRC_32` is 0x2AB104B2
	const std::vector<uint8_t> bytes = {0x00, 0xB0, 0x0D, 0x00, 0x01, 0xC1, 0x00, 0x00, 0x00, 0x01, 0xF0, 0x00, 0x2A, 0xB1, 0x04, 0xB2};

	const auto pat					 = ParsePat(bytes);
	ASSERT_NE(pat, nullptr);
	EXPECT_EQ(pat->_program_map_pid, 0x1000);
}

TEST(MpegTsSectionTest, RejectsSectionWithWrongCrc)
{
	for (auto bytes : {MakePatSectionBytes({{1, 0x1000}}), MakePmtSectionBytes(0)})
	{
		SCOPED_TRACE(static_cast<int>(bytes[0]));

		bytes.back() ^= 0x01;

		mpegts::Section section(0x0000);
		EXPECT_EQ(section.AppendData(bytes.data(), bytes.size()), bytes.size());
		EXPECT_FALSE(section.IsCompleted());
		EXPECT_TRUE(section.IsFailed());
	}
}

// PAT and PMT carry a `CRC_32` whatever their `section_syntax_indicator` says, so clearing the bit does not skip the check.
TEST(MpegTsSectionTest, RejectsPsiWithWrongCrcAndClearedSyntaxIndicator)
{
	for (auto bytes : {MakePatSectionBytes({{1, 0x1000}}), MakePmtSectionBytes(0)})
	{
		SCOPED_TRACE(static_cast<int>(bytes[0]));

		// `section_syntax_indicator` is the top bit of the second byte; the `CRC_32` is left as it was
		bytes[1] &= 0x7F;

		mpegts::Section section(0x0000);
		EXPECT_EQ(section.AppendData(bytes.data(), bytes.size()), bytes.size());
		EXPECT_FALSE(section.IsCompleted());
		EXPECT_TRUE(section.IsFailed());
		EXPECT_TRUE(section.HasCrcMismatch());
	}
}
