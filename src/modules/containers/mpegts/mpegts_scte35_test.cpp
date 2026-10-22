//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2026 OvenMediaLabs. All rights reserved.
//
//==============================================================================
#include <base/modules/data_format/scte35_event/scte35_event.h>
#include <base/ovlibrary/crc.h>
#include <gtest/gtest.h>

#include <optional>
#include <vector>

#include "mpegts_section.h"
#include "scte35/mpegts_splice_insert.h"

namespace
{
	constexpr uint16_t SCTE35_PID		 = 0x0102;
	constexpr uint64_t PTS_MASK			 = 0x1FFFFFFFF;
	constexpr uint32_t SPLICE_EVENT_ID	 = 0x12345678;
	constexpr uint64_t BREAK_DURATION	 = 90000 * 30;
	constexpr uint8_t SPLICE_INSERT_TYPE = 0x05;

	void AppendBe(std::vector<uint8_t> &bytes, uint64_t value, size_t size)
	{
		for (size_t i = 0; i < size; i++)
		{
			bytes.push_back(static_cast<uint8_t>(value >> (8 * (size - 1 - i))));
		}
	}

	// `splice_time()` with `time_specified_flag`=1
	void AppendSpliceTime(std::vector<uint8_t> &bytes, uint64_t pts)
	{
		bytes.push_back(static_cast<uint8_t>(0xFE | ((pts >> 32) & 0x01)));
		AppendBe(bytes, pts & 0xFFFFFFFF, 4);
	}

	struct SpliceInsertFields
	{
		bool cancel							= false;
		bool out_of_network					= true;
		bool program_splice					= true;
		bool immediate						= false;
		std::optional<uint64_t> pts			= 90000 * 10;
		std::optional<uint64_t> duration	= BREAK_DURATION;
		// `component_tag` per component when program_splice is false; each gets `pts` as its splice time
		std::vector<uint8_t> component_tags = {};
	};

	std::vector<uint8_t> MakeSpliceInsertCommand(const SpliceInsertFields &fields)
	{
		std::vector<uint8_t> bytes;

		AppendBe(bytes, SPLICE_EVENT_ID, 4);
		bytes.push_back(fields.cancel ? 0xFF : 0x7F);

		if (fields.cancel)
		{
			return bytes;
		}

		bytes.push_back(static_cast<uint8_t>((fields.out_of_network ? 0x80 : 0x00) | (fields.program_splice ? 0x40 : 0x00) |
											 (fields.duration.has_value() ? 0x20 : 0x00) | (fields.immediate ? 0x10 : 0x00) | 0x0F));

		if (fields.program_splice && (fields.immediate == false))
		{
			if (fields.pts.has_value())
			{
				AppendSpliceTime(bytes, fields.pts.value());
			}
			else
			{
				bytes.push_back(0x7F);
			}
		}

		if (fields.program_splice == false)
		{
			bytes.push_back(static_cast<uint8_t>(fields.component_tags.size()));

			for (const auto tag : fields.component_tags)
			{
				bytes.push_back(tag);

				if (fields.immediate == false)
				{
					AppendSpliceTime(bytes, fields.pts.value());
				}
			}
		}

		if (fields.duration.has_value())
		{
			bytes.push_back(static_cast<uint8_t>(0x80 | 0x7E | ((fields.duration.value() >> 32) & 0x01)));
			AppendBe(bytes, fields.duration.value() & 0xFFFFFFFF, 4);
		}

		// unique_program_id, avail_num, avails_expected
		bytes.insert(bytes.end(), {0x00, 0x01, 0x00, 0x00});

		return bytes;
	}

	struct SpliceInfoFields
	{
		uint8_t sap_type							   = 0x03;
		bool encrypted_packet						   = false;
		uint64_t pts_adjustment						   = 0;
		std::optional<uint16_t> splice_command_length  = std::nullopt;
		std::vector<uint8_t> descriptors			   = {};
		std::optional<uint16_t> descriptor_loop_length = std::nullopt;
	};

	// A `splice_info_section` carrying `command`, with the lengths taken from the data unless given
	std::vector<uint8_t> MakeSpliceInfoSection(uint8_t command_type, const std::vector<uint8_t> &command, const SpliceInfoFields &fields = {})
	{
		std::vector<uint8_t> body;

		// protocol_version
		body.push_back(0x00);
		// `encrypted_packet`, `encryption_algorithm`=0, `pts_adjustment`
		body.push_back(static_cast<uint8_t>((fields.encrypted_packet ? 0x80 : 0x00) | ((fields.pts_adjustment >> 32) & 0x01)));
		AppendBe(body, fields.pts_adjustment & 0xFFFFFFFF, 4);
		// cw_index
		body.push_back(0xFF);
		// `tier`=0xFFF, `splice_command_length`
		const uint16_t command_length = fields.splice_command_length.value_or(static_cast<uint16_t>(command.size()));
		AppendBe(body, (0xFFFull << 12) | command_length, 3);
		body.push_back(command_type);
		body.insert(body.end(), command.begin(), command.end());
		AppendBe(body, fields.descriptor_loop_length.value_or(static_cast<uint16_t>(fields.descriptors.size())), 2);
		body.insert(body.end(), fields.descriptors.begin(), fields.descriptors.end());

		const auto section_length  = body.size() + 4;

		// `table_id`, `section_syntax_indicator`=0, `private_indicator`=0, `sap_type`, `section_length` (12 bits)
		std::vector<uint8_t> bytes = {0xFC, static_cast<uint8_t>(((fields.sap_type & 0x03) << 4) | ((section_length >> 8) & 0x0F)), static_cast<uint8_t>(section_length & 0xFF)};
		bytes.insert(bytes.end(), body.begin(), body.end());
		AppendBe(bytes, ov::CRC::Crc32Mpeg2(bytes.data(), bytes.size()), 4);

		return bytes;
	}

	std::shared_ptr<mpegts::Section> ParseSection(const std::vector<uint8_t> &bytes)
	{
		auto section = std::make_shared<mpegts::Section>(SCTE35_PID);
		EXPECT_EQ(section->AppendData(bytes.data(), bytes.size()), bytes.size());

		return section;
	}

	std::shared_ptr<const mpegts::SpliceInsert> ParseSpliceInsert(const std::vector<uint8_t> &bytes)
	{
		auto section = ParseSection(bytes);
		if (section->IsCompleted() == false)
		{
			return nullptr;
		}

		return std::dynamic_pointer_cast<const mpegts::SpliceInsert>(section->GetSpliceInfo());
	}

	std::shared_ptr<Scte35Event> CreateEvent(const SpliceInsertFields &command, const SpliceInfoFields &fields = {})
	{
		auto section = ParseSection(MakeSpliceInfoSection(SPLICE_INSERT_TYPE, MakeSpliceInsertCommand(command), fields));
		if (section->IsCompleted() == false)
		{
			ADD_FAILURE() << "splice_info_section is not parsed";
			return nullptr;
		}

		return Scte35Event::Create(section->GetSpliceInfo());
	}
}  // namespace

TEST(MpegTsScte35Test, ParsesProgramSpliceInsert)
{
	const auto splice_insert = ParseSpliceInsert(MakeSpliceInfoSection(SPLICE_INSERT_TYPE, MakeSpliceInsertCommand({})));

	ASSERT_NE(splice_insert, nullptr);
	EXPECT_EQ(splice_insert->GetSpliceEventId(), SPLICE_EVENT_ID);
	EXPECT_EQ(splice_insert->GetPTSTime(), 90000 * 10);
	EXPECT_EQ(splice_insert->GetDuration(), BREAK_DURATION);
}

// An immediate `splice_insert` has no `splice_time()`, so `break_duration()` follows the flags.
TEST(MpegTsScte35Test, ParsesImmediateSpliceInsert)
{
	SpliceInsertFields fields;
	fields.immediate		 = true;

	const auto splice_insert = ParseSpliceInsert(MakeSpliceInfoSection(SPLICE_INSERT_TYPE, MakeSpliceInsertCommand(fields)));

	ASSERT_NE(splice_insert, nullptr);
	EXPECT_EQ(splice_insert->GetSpliceImmediateFlag(), 1);
	EXPECT_EQ(splice_insert->GetDuration(), BREAK_DURATION);
}

// A component `splice_insert` carries a component loop before `break_duration()`.
TEST(MpegTsScte35Test, ParsesComponentSpliceInsert)
{
	SpliceInsertFields fields;
	fields.program_splice	 = false;
	fields.component_tags	 = {0x01, 0x02};

	const auto splice_insert = ParseSpliceInsert(MakeSpliceInfoSection(SPLICE_INSERT_TYPE, MakeSpliceInsertCommand(fields)));

	ASSERT_NE(splice_insert, nullptr);
	EXPECT_EQ(splice_insert->GetProgramSpliceFlag(), 0);
	EXPECT_EQ(splice_insert->GetDuration(), BREAK_DURATION);
}

TEST(MpegTsScte35Test, ParsesSpliceInfoWithDescriptors)
{
	SpliceInfoFields info;
	// avail_descriptor
	info.descriptors		 = {0x00, 0x08, 'C', 'U', 'E', 'I', 0x00, 0x00, 0x00, 0x01};

	const auto splice_insert = ParseSpliceInsert(MakeSpliceInfoSection(SPLICE_INSERT_TYPE, MakeSpliceInsertCommand({}), info));

	ASSERT_NE(splice_insert, nullptr);
	EXPECT_EQ(splice_insert->GetDuration(), BREAK_DURATION);
}

// `sap_type` 0 to 2 signal a stream access point and are as valid as 3 (not specified).
TEST(MpegTsScte35Test, AcceptsEverySapType)
{
	for (uint8_t sap_type = 0; sap_type <= 3; sap_type++)
	{
		SCOPED_TRACE(static_cast<int>(sap_type));

		SpliceInfoFields info;
		info.sap_type = sap_type;

		EXPECT_NE(ParseSpliceInsert(MakeSpliceInfoSection(SPLICE_INSERT_TYPE, MakeSpliceInsertCommand({}), info)), nullptr);
	}
}

// `splice_info_section` allows a `section_length` of up to 4093 bytes.
TEST(MpegTsScte35Test, AcceptsSectionLongerThanPsiLimit)
{
	SpliceInfoFields info;
	// One private descriptor per 255 bytes until the section passes 1021 bytes
	for (int i = 0; i < 5; i++)
	{
		info.descriptors.insert(info.descriptors.end(), {0xFF, 253, 'C', 'U', 'E', 'I'});
		info.descriptors.insert(info.descriptors.end(), 249, 0x00);
	}

	const auto bytes = MakeSpliceInfoSection(SPLICE_INSERT_TYPE, MakeSpliceInsertCommand({}), info);
	ASSERT_GT(bytes.size() - 3, 1021u);

	const auto splice_insert = ParseSpliceInsert(bytes);
	ASSERT_NE(splice_insert, nullptr);
	EXPECT_EQ(splice_insert->GetDuration(), BREAK_DURATION);
}

TEST(MpegTsScte35Test, RejectsSectionLongerThan4093)
{
	// 15 descriptors of 255 bytes and one of 232 bytes make `section_length` 4094
	SpliceInfoFields info;
	for (int i = 0; i < 15; i++)
	{
		info.descriptors.insert(info.descriptors.end(), {0xFF, 253, 'C', 'U', 'E', 'I'});
		info.descriptors.insert(info.descriptors.end(), 249, 0x00);
	}
	info.descriptors.insert(info.descriptors.end(), {0xFF, 230, 'C', 'U', 'E', 'I'});
	info.descriptors.insert(info.descriptors.end(), 226, 0x00);

	const auto bytes = MakeSpliceInfoSection(SPLICE_INSERT_TYPE, MakeSpliceInsertCommand({}), info);
	ASSERT_EQ(bytes.size() - 3, 4094u);

	mpegts::Section section(SCTE35_PID);
	EXPECT_EQ(section.AppendData(bytes.data(), bytes.size()), 0u);
}

// `splice_command_length` 0xFFF is a legacy value to be ignored, so the command is read by its own syntax.
TEST(MpegTsScte35Test, ParsesLegacyCommandLength)
{
	for (const bool with_descriptors : {false, true})
	{
		SCOPED_TRACE(with_descriptors);

		SpliceInfoFields info;
		info.splice_command_length = 0xFFF;
		if (with_descriptors)
		{
			// avail_descriptor
			info.descriptors = {0x00, 0x08, 'C', 'U', 'E', 'I', 0x00, 0x00, 0x00, 0x01};
		}

		const auto splice_insert = ParseSpliceInsert(MakeSpliceInfoSection(SPLICE_INSERT_TYPE, MakeSpliceInsertCommand({}), info));

		ASSERT_NE(splice_insert, nullptr);
		EXPECT_EQ(splice_insert->GetPTSTime(), 90000 * 10);
		EXPECT_EQ(splice_insert->GetDuration(), BREAK_DURATION);
	}
}

TEST(MpegTsScte35Test, RejectsLegacyCommandLengthWithTruncatedCommand)
{
	SpliceInfoFields info;
	info.splice_command_length = 0xFFF;

	auto command			   = MakeSpliceInsertCommand({});
	command.resize(command.size() - 3);

	// Without the `descriptor_loop_length` after it, the command runs into the CRC
	EXPECT_TRUE(ParseSection(MakeSpliceInfoSection(SPLICE_INSERT_TYPE, command, info))->IsFailed());
}

TEST(MpegTsScte35Test, RejectsCommandShorterThanItsFields)
{
	auto command = MakeSpliceInsertCommand({});
	command.pop_back();

	EXPECT_TRUE(ParseSection(MakeSpliceInfoSection(SPLICE_INSERT_TYPE, command))->IsFailed());
}

TEST(MpegTsScte35Test, RejectsCommandLengthPastSection)
{
	SpliceInfoFields info;
	info.splice_command_length = static_cast<uint16_t>(MakeSpliceInsertCommand({}).size() + 8);

	EXPECT_TRUE(ParseSection(MakeSpliceInfoSection(SPLICE_INSERT_TYPE, MakeSpliceInsertCommand({}), info))->IsFailed());
}

TEST(MpegTsScte35Test, RejectsDescriptorLoopPastSection)
{
	SpliceInfoFields info;
	info.descriptor_loop_length = 10;

	EXPECT_TRUE(ParseSection(MakeSpliceInfoSection(SPLICE_INSERT_TYPE, MakeSpliceInsertCommand({}), info))->IsFailed());
}

// An encrypted `splice_command()` cannot be read as a plain one.
TEST(MpegTsScte35Test, RejectsEncryptedSpliceInfo)
{
	SpliceInfoFields info;
	info.encrypted_packet = true;

	EXPECT_TRUE(ParseSection(MakeSpliceInfoSection(SPLICE_INSERT_TYPE, MakeSpliceInsertCommand({}), info))->IsFailed());
}

TEST(MpegTsScte35Test, EventCarriesSpliceTime)
{
	const auto event = CreateEvent({});

	ASSERT_NE(event, nullptr);
	EXPECT_EQ(event->GetID(), SPLICE_EVENT_ID);
	EXPECT_TRUE(event->IsOutOfNetwork());
	EXPECT_EQ(event->GetTimestampMsec(), 10000);
	EXPECT_EQ(event->GetDurationMsec(), 30000);
}

TEST(MpegTsScte35Test, CancelCreatesNoEvent)
{
	SpliceInsertFields fields;
	fields.cancel = true;

	EXPECT_EQ(CreateEvent(fields), nullptr);
}

TEST(MpegTsScte35Test, SpliceInsertWithoutSpliceTimeCreatesNoEvent)
{
	SpliceInsertFields unspecified;
	unspecified.pts = std::nullopt;
	EXPECT_EQ(CreateEvent(unspecified), nullptr);

	SpliceInsertFields component;
	component.program_splice = false;
	component.component_tags = {0x01};
	EXPECT_EQ(CreateEvent(component), nullptr);
}

// An immediate `splice_insert` has no time to carry, so its event leaves the time to the caller.
TEST(MpegTsScte35Test, ImmediateSpliceInsertCreatesEventWithoutTime)
{
	SpliceInsertFields immediate;
	immediate.immediate = true;

	const auto event	= CreateEvent(immediate);

	ASSERT_NE(event, nullptr);
	EXPECT_EQ(event->GetID(), SPLICE_EVENT_ID);
	EXPECT_TRUE(event->IsOutOfNetwork());
	EXPECT_EQ(event->GetTimestampMsec(), 0);
	EXPECT_EQ(event->GetDurationMsec(), 30000);
}

TEST(MpegTsScte35Test, EventAddsPtsAdjustment)
{
	SpliceInfoFields info;
	info.pts_adjustment = 90000 * 5;

	const auto event	= CreateEvent({}, info);

	ASSERT_NE(event, nullptr);
	EXPECT_EQ(event->GetTimestampMsec(), 15000);
}

TEST(MpegTsScte35Test, EventWrapsPtsAdjustmentAt33Bits)
{
	SpliceInsertFields fields;
	fields.pts = PTS_MASK + 1 - 90000;

	SpliceInfoFields info;
	info.pts_adjustment = 90000 * 2;

	const auto event	= CreateEvent(fields, info);

	ASSERT_NE(event, nullptr);
	EXPECT_EQ(event->GetTimestampMsec(), 1000);
}

// A section that OME writes for an `Scte35Event` reads back as the same event.
TEST(MpegTsScte35Test, BuiltSectionParsesBack)
{
	const auto built = Scte35Event::Create(mpegts::SpliceCommandType::SPLICE_INSERT, SPLICE_EVENT_ID, true, 12345, 30000, true);
	const auto data	 = built->MakeScteData();
	ASSERT_NE(data, nullptr);

	mpegts::Section section(SCTE35_PID);
	ASSERT_EQ(section.AppendData(data->GetDataAs<uint8_t>(), data->GetLength()), data->GetLength());
	ASSERT_TRUE(section.IsCompleted());

	const auto event = Scte35Event::Create(section.GetSpliceInfo());
	ASSERT_NE(event, nullptr);
	EXPECT_EQ(event->GetID(), SPLICE_EVENT_ID);
	EXPECT_TRUE(event->IsOutOfNetwork());
	EXPECT_EQ(event->GetTimestampMsec(), 12345);
	EXPECT_EQ(event->GetDurationMsec(), 30000);
	EXPECT_TRUE(event->IsAutoReturn());
}
