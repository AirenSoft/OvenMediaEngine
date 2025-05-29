//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2025 AirenSoft. All rights reserved.
//
//==============================================================================
#include "./rtmp_video_track.h"

#include "../rtmp_provider_private.h"

namespace pvd::rtmp
{
	cmn::PacketType RtmpVideoTrack::ToCommonPacketType(modules::flv::VideoPacketType packet_type)
	{
		switch (packet_type)
		{
			OV_CASE_RETURN(modules::flv::VideoPacketType::SequenceStart, cmn::PacketType::SEQUENCE_HEADER);
			OV_CASE_RETURN(modules::flv::VideoPacketType::CodedFrames, cmn::PacketType::NALU);
			OV_CASE_RETURN(modules::flv::VideoPacketType::CodedFramesX, cmn::PacketType::NALU);

			case modules::flv::VideoPacketType::Metadata:
				// Metadata should be handled in the stream
				OV_ASSERT(false, "VideoPacketType::Metadata should be handled in the RTMP stream");
				[[fallthrough]];
			case modules::flv::VideoPacketType::SequenceEnd:
				[[fallthrough]];
			case modules::flv::VideoPacketType::MPEG2TSSequenceStart:
				[[fallthrough]];
			case modules::flv::VideoPacketType::Multitrack:
				[[fallthrough]];
			case modules::flv::VideoPacketType::ModEx:
				logtd("Not handled video packet type: %s", modules::flv::EnumToString(packet_type));
				return cmn::PacketType::Unknown;
		}

		OV_ASSERT(false, "Unknown VideoPacketType");
		return cmn::PacketType::Unknown;
	}
}  // namespace pvd::rtmp
