//==============================================================================
//
//  OvenMediaEngine - Unit Tests
//
//  Covers: the codec parameter fields (codecs, profile, level, tier, bitDepth)
//  of a track in serdes::JsonFromTrack()
//
//==============================================================================
#include <gtest/gtest.h>
#include <modules/bitstream/av1/av1_decoder_configuration_record.h>
#include <modules/json_serdes/stream.h>

#include <vector>

namespace
{
	const char *kCodecParameterKeys[] = {"codecs", "profile", "level", "tier", "bitDepth"};

	std::shared_ptr<MediaTrack> MakeVideoTrack(cmn::MediaCodecId codec_id)
	{
		auto track = std::make_shared<MediaTrack>();
		track->SetMediaType(cmn::MediaType::Video);
		track->SetCodecId(codec_id);
		track->SetTimeBase(1, 90000);
		track->SetResolution(640, 360);
		return track;
	}

	std::shared_ptr<MediaTrack> MakeAudioTrack(cmn::MediaCodecId codec_id, int32_t sample_rate)
	{
		auto track = std::make_shared<MediaTrack>();
		track->SetMediaType(cmn::MediaType::Audio);
		track->SetCodecId(codec_id);
		track->SetTimeBase(1, sample_rate);
		track->SetSampleRate(sample_rate);
		track->SetChannelLayout(cmn::AudioChannel::Layout::LayoutStereo);
		return track;
	}

	// av1C of seq_profile 0, seq_level_idx 8 (4.0), Main tier, 10 bit 4:2:0
	std::shared_ptr<AV1DecoderConfigurationRecord> MakeAv1Config()
	{
		const std::vector<uint8_t> av1c = {0x81, 0x08, 0x4C, 0x00};

		auto config = std::make_shared<AV1DecoderConfigurationRecord>();
		if (config->Parse(std::make_shared<ov::Data>(av1c.data(), av1c.size())) == false)
		{
			return nullptr;
		}

		return config;
	}
}  // namespace

// The resolution can be known before the sequence header (onMetaData over RTMP), and the track is
// not ready until the decoder configuration arrives
TEST(JsonSerdesTrack, ReportsTheCodecParametersOnlyOnceTheTrackIsReady)
{
	auto track = MakeVideoTrack(cmn::MediaCodecId::Av1);
	ASSERT_FALSE(track->IsValid());

	auto video = serdes::JsonFromTrack(track)["video"];
	for (auto key : kCodecParameterKeys)
	{
		EXPECT_FALSE(video.isMember(key)) << key;
	}

	auto config = MakeAv1Config();
	ASSERT_NE(config, nullptr);
	track->SetDecoderConfigurationRecord(config);
	ASSERT_TRUE(track->IsValid());

	video = serdes::JsonFromTrack(track)["video"];
	EXPECT_EQ(video["codecs"].asString(), "av01.0.08M.10");
	EXPECT_EQ(video["profile"].asString(), "Main");
	EXPECT_EQ(video["level"].asString(), "4.0");
	EXPECT_EQ(video["tier"].asString(), "Main");
	EXPECT_EQ(video["bitDepth"].asInt(), 10);
}

// MPEG audio from MPEG-TS is any layer, so its codecs string names the MPEG version by the sample rate
TEST(JsonSerdesTrack, ReportsTheCodecsOfMpegAudio)
{
	struct Case
	{
		cmn::MediaCodecId codec_id;
		int32_t sample_rate;
		const char *codecs;
	};

	for (const auto &test : {Case{cmn::MediaCodecId::Mp3, 44100, "mp4a.40.34"},
							 Case{cmn::MediaCodecId::Mp3, 22050, "mp4a.40.34"},
							 Case{cmn::MediaCodecId::Mp2, 48000, "mp4a.6B"},
							 Case{cmn::MediaCodecId::Mp2, 24000, "mp4a.69"},
							 // MPEG-2.5 is not an ISO/IEC version
							 Case{cmn::MediaCodecId::Mp3, 11025, nullptr},
							 Case{cmn::MediaCodecId::Mp2, 11025, nullptr}})
	{
		auto track = MakeAudioTrack(test.codec_id, test.sample_rate);
		ASSERT_TRUE(track->IsValid()) << test.sample_rate;

		auto audio = serdes::JsonFromTrack(track)["audio"];
		if (test.codecs == nullptr)
		{
			EXPECT_FALSE(audio.isMember("codecs")) << test.sample_rate;
		}
		else
		{
			EXPECT_EQ(audio["codecs"].asString(), test.codecs) << test.sample_rate;
		}
	}
}

// An image track is told apart by its codec alone
TEST(JsonSerdesTrack, ReportsNoCodecsForAnImageTrack)
{
	for (auto codec_id : {cmn::MediaCodecId::Jpeg, cmn::MediaCodecId::Png, cmn::MediaCodecId::Webp})
	{
		auto track = MakeVideoTrack(codec_id);
		ASSERT_TRUE(track->IsValid());

		auto video = serdes::JsonFromTrack(track)["video"];
		for (auto key : kCodecParameterKeys)
		{
			EXPECT_FALSE(video.isMember(key)) << key;
		}
	}
}
