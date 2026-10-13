//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2026 OvenMediaLabs. All rights reserved.
//
//==============================================================================
#include "rtp_packetizer.h"

#include <gtest/gtest.h>

#include "rtp_packet.h"

namespace
{
	// Collects what the packetizer hands the session
	class CollectingSession : public RtpPacketizerInterface
	{
	public:
		bool OnRtpPacketized(std::shared_ptr<RtpPacket> packet) override
		{
			packets.push_back(packet);
			return true;
		}

		std::vector<std::shared_ptr<RtpPacket>> packets;
	};
}  // namespace

// `RtpFrameBoundaryDetector` runs in `RtpRtcp::OnRtpReceived()`, so it never sees a packet this
// side makes. Without the packetizer setting them, every sent packet answers `false` to both.
TEST(RtpPacketizerFrameBoundary, AVideoFrameIsMarkedAtItsFirstAndLastPacket)
{
	auto session = std::make_shared<CollectingSession>();
	RtpPacketizer packetizer(session);

	ASSERT_TRUE(packetizer.SetCodec(cmn::MediaCodecId::H264));
	packetizer.SetTrackId(1);
	packetizer.SetPayloadType(96);
	packetizer.SetSSRC(0x11223344);

	// One NAL larger than a packet's payload, so the frame is fragmented and only one packet ends it
	std::vector<uint8_t> frame(4000, 0x00);
	frame[0] = 0x65;  // IDR slice

	FragmentationHeader fragmentation;
	fragmentation.AddFragment(0, frame.size());

	RTPVideoHeader video_header						  = {};
	video_header.codec								  = cmn::MediaCodecId::H264;
	video_header.codec_header.h26X.packetization_mode = H26XPacketizationMode::NonInterleaved;
	video_header.codec_header.h26X.nalus_length		  = 0;

	ASSERT_TRUE(packetizer.Packetize(FrameType::VideoFrameKey, 90000, 0, frame.data(), frame.size(),
									 &fragmentation, &video_header));

	ASSERT_GT(session->packets.size(), 1u);

	for (size_t i = 0; i < session->packets.size(); i++)
	{
		const bool last = ((i + 1) == session->packets.size());
		EXPECT_EQ(session->packets[i]->IsFirstPacketOfFrame(), (i == 0)) << "packet " << i;
		EXPECT_EQ(session->packets[i]->IsLastPacketOfFrame(), last) << "packet " << i;
	}
}

// One audio access unit is one packet, so it both starts and ends its frame
TEST(RtpPacketizerFrameBoundary, AnAudioPacketIsAWholeFrame)
{
	auto session = std::make_shared<CollectingSession>();
	RtpPacketizer packetizer(session);

	ASSERT_TRUE(packetizer.SetCodec(cmn::MediaCodecId::Opus));
	packetizer.SetTrackId(2);
	packetizer.SetPayloadType(111);
	packetizer.SetSSRC(0x55667788);

	const std::vector<uint8_t> frame(160, 0x7F);
	ASSERT_TRUE(packetizer.Packetize(FrameType::AudioFrameSpeech, 48000, 0, frame.data(), frame.size(),
									 nullptr, nullptr));

	ASSERT_EQ(session->packets.size(), 1u);
	EXPECT_TRUE(session->packets[0]->IsFirstPacketOfFrame());
	EXPECT_TRUE(session->packets[0]->IsLastPacketOfFrame());
}
