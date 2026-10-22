//==============================================================================
//
//  OvenMediaEngine - Unit Tests
//
//  Covers: AacAdtsSplitter splitting a buffer of ADTS frames into one entry per AAC frame
//
//==============================================================================
#include <gtest/gtest.h>
#include <modules/bitstream/aac/aac_adts_splitter.h>

#include <vector>

namespace
{
	constexpr int32_t TIMESCALE_90K = 90000;
	constexpr uint8_t SAMPLING_INDEX_48000 = 3;
	constexpr uint8_t SAMPLING_INDEX_44100 = 4;

	// One AAC-LC stereo ADTS frame without CRC, `frame_length` bytes in all.
	std::vector<uint8_t> AdtsFrame(size_t frame_length, uint8_t sampling_index = SAMPLING_INDEX_48000)
	{
		constexpr uint8_t PROFILE_LC = 1;
		constexpr uint8_t CHANNELS = 2;

		std::vector<uint8_t> frame(frame_length, 0x21);

		frame[0] = 0xFF;
		frame[1] = 0xF1;  // MPEG-4, layer 0, `protection_absent`
		frame[2] = static_cast<uint8_t>((PROFILE_LC << 6) | (sampling_index << 2) | (CHANNELS >> 2));
		frame[3] = static_cast<uint8_t>(((CHANNELS & 0x03) << 6) | ((frame_length >> 11) & 0x03));
		frame[4] = static_cast<uint8_t>((frame_length >> 3) & 0xFF);
		frame[5] = static_cast<uint8_t>(((frame_length & 0x07) << 5) | 0x1F);
		frame[6] = 0xFC;

		return frame;
	}

	std::vector<uint8_t> Concat(const std::vector<std::vector<uint8_t>> &frames)
	{
		std::vector<uint8_t> buffer;
		for (const auto &frame : frames)
		{
			buffer.insert(buffer.end(), frame.begin(), frame.end());
		}
		return buffer;
	}

	AacAdtsSplitter::Result Split(const std::vector<uint8_t> &buffer, int32_t timescale = TIMESCALE_90K, int samples_per_frame = 1024)
	{
		return AacAdtsSplitter::Split(buffer.data(), buffer.size(), timescale, samples_per_frame);
	}
}  // namespace

TEST(AacAdtsSplitter, PesOfManyFramesGivesOneEntryPerFrame)
{
	constexpr int FRAME_COUNT = 15;
	constexpr int64_t FRAME_DURATION_48K_IN_90K = 1920;

	std::vector<std::vector<uint8_t>> frames;
	for (int i = 0; i < FRAME_COUNT; i++)
	{
		frames.push_back(AdtsFrame(300 + i));
	}

	const auto buffer = Concat(frames);
	const auto result = Split(buffer);

	ASSERT_EQ(result.frames.size(), static_cast<size_t>(FRAME_COUNT));
	EXPECT_TRUE(result.stop_reason.IsEmpty());

	size_t offset = 0;
	for (int i = 0; i < FRAME_COUNT; i++)
	{
		EXPECT_EQ(result.frames[i].offset, offset) << "frame " << i;
		EXPECT_EQ(result.frames[i].length, frames[i].size()) << "frame " << i;
		EXPECT_EQ(result.frames[i].timestamp_offset, i * FRAME_DURATION_48K_IN_90K) << "frame " << i;
		offset += frames[i].size();
	}
}

TEST(AacAdtsSplitter, FrameDurationIsRoundedOnceAndMultiplied)
{
	// 1024 samples at 44.1 kHz are 2089.8 ticks of 90 kHz.
	constexpr int64_t FRAME_DURATION_44K_IN_90K = 2090;

	const auto buffer = Concat({AdtsFrame(200, SAMPLING_INDEX_44100), AdtsFrame(200, SAMPLING_INDEX_44100), AdtsFrame(200, SAMPLING_INDEX_44100)});
	const auto result = Split(buffer);

	ASSERT_EQ(result.frames.size(), 3u);
	EXPECT_EQ(result.frames[1].timestamp_offset, FRAME_DURATION_44K_IN_90K);
	EXPECT_EQ(result.frames[2].timestamp_offset, 2 * FRAME_DURATION_44K_IN_90K);
}

TEST(AacAdtsSplitter, TimestampsFollowTheTimescaleAndSamplesPerFrame)
{
	const auto buffer = Concat({AdtsFrame(100), AdtsFrame(100)});

	// The track's own timescale of 48 kHz counts samples.
	EXPECT_EQ(Split(buffer, 48000).frames.at(1).timestamp_offset, 1024);
	// A non-positive `samples_per_frame` means 1024.
	EXPECT_EQ(Split(buffer, 48000, 0).frames.at(1).timestamp_offset, 1024);
	EXPECT_EQ(Split(buffer, 48000, 960).frames.at(1).timestamp_offset, 960);
}

TEST(AacAdtsSplitter, SingleFrameIsOneEntry)
{
	const auto result = Split(AdtsFrame(256));

	ASSERT_EQ(result.frames.size(), 1u);
	EXPECT_EQ(result.frames[0].offset, 0u);
	EXPECT_EQ(result.frames[0].length, 256u);
	EXPECT_EQ(result.frames[0].timestamp_offset, 0);
	EXPECT_TRUE(result.stop_reason.IsEmpty());
}

TEST(AacAdtsSplitter, UnparsableFirstFrameGivesNoEntry)
{
	auto buffer = Concat({AdtsFrame(100), AdtsFrame(100)});
	buffer[0] = 0x00;

	const auto result = Split(buffer);

	EXPECT_TRUE(result.frames.empty());
	EXPECT_FALSE(result.stop_reason.IsEmpty());
}

TEST(AacAdtsSplitter, BufferShorterThanAHeaderGivesNoEntryAndNoReason)
{
	const std::vector<uint8_t> buffer = {0xFF, 0xF1, 0x4C};
	const auto result = Split(buffer);

	EXPECT_TRUE(result.frames.empty());
	EXPECT_TRUE(result.stop_reason.IsEmpty());
}

TEST(AacAdtsSplitter, GarbageMidBufferKeepsTheFramesBeforeIt)
{
	auto buffer = Concat({AdtsFrame(100), AdtsFrame(100), AdtsFrame(100)});
	buffer[200] = 0x00;

	const auto result = Split(buffer);

	ASSERT_EQ(result.frames.size(), 2u);
	EXPECT_EQ(result.frames[1].offset, 100u);
	EXPECT_FALSE(result.stop_reason.IsEmpty());
}

TEST(AacAdtsSplitter, FrameLongerThanTheBufferStopsTheSplit)
{
	auto buffer = Concat({AdtsFrame(100), AdtsFrame(100)});
	// The second frame claims more bytes than remain.
	buffer.resize(150);

	const auto result = Split(buffer);

	ASSERT_EQ(result.frames.size(), 1u);
	EXPECT_EQ(result.frames[0].length, 100u);
	EXPECT_FALSE(result.stop_reason.IsEmpty());
}

TEST(AacAdtsSplitter, FrameLengthBelowTheHeaderSizeStopsTheSplit)
{
	auto buffer = AdtsFrame(100);
	// `aac_frame_length` of 0
	buffer[3] &= 0xFC;
	buffer[4] = 0x00;
	buffer[5] &= 0x1F;

	const auto result = Split(buffer);

	EXPECT_TRUE(result.frames.empty());
	EXPECT_FALSE(result.stop_reason.IsEmpty());
}

TEST(AacAdtsSplitter, TrailingBytesShorterThanAHeaderAreReported)
{
	auto buffer = Concat({AdtsFrame(100), AdtsFrame(100)});
	buffer.insert(buffer.end(), {0x01, 0x02, 0x03});

	const auto result = Split(buffer);

	ASSERT_EQ(result.frames.size(), 2u);
	EXPECT_FALSE(result.stop_reason.IsEmpty());
}

TEST(AacAdtsSplitter, UntimeableFramesAreNotSplit)
{
	const auto buffer = Concat({AdtsFrame(100), AdtsFrame(100)});

	// A zero timescale gives a frame duration of 0.
	const auto result = Split(buffer, 0);

	EXPECT_TRUE(result.frames.empty());
	EXPECT_FALSE(result.stop_reason.IsEmpty());
}
