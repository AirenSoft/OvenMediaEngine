//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2026 OvenMediaLabs. All rights reserved.
//
//==============================================================================
#include "mpegts_section.h"

#include <gtest/gtest.h>

#include <vector>

namespace
{
	constexpr uint16_t PMT_PID = 0x1000;

	// A PMT section for program 1 with one H.264 ES on PID 0x0100, then `extra_bytes` bytes and the CRC field.
	// The extra bytes and the CRC field are zero, since Section does not verify the CRC.
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

		bytes.insert(bytes.end(), extra_bytes + 4, 0x00);

		return bytes;
	}
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
