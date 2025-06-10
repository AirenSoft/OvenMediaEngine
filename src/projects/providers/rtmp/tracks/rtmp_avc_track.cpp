//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2025 AirenSoft. All rights reserved.
//
//==============================================================================
#include "./rtmp_avc_track.h"

#include <modules/rtmp_v2/chunk/rtmp_datastructure.h>

#include "../rtmp_provider_private.h"
#include "../rtmp_stream_v2.h"

namespace pvd::rtmp
{
	bool RtmpAvcTrack::Handle(
		const std::shared_ptr<const modules::rtmp::Message> &message,
		const modules::flv::ParserCommon &parser,
		const std::shared_ptr<const modules::flv::CommonData> &data)
	{
		do
		{
			auto video_data = std::dynamic_pointer_cast<const modules::flv::VideoData>(data);

			if (video_data == nullptr)
			{
				// This should never happen
				OV_ASSERT2(false);
				return false;
			}

			auto packet_type = ToCommonPacketType(video_data->video_packet_type);

			if (packet_type == cmn::PacketType::Unknown)
			{
				break;
			}

			// CTS = PTS - DTS
			auto cts = video_data->composition_time_offset;
			int64_t dts = message->header->completed.timestamp;
			int64_t pts = cts + dts;

			AdjustTimestamp(pts, dts);
			_last_pts = dts;

			std::shared_ptr<MediaPacket> media_packet;

			if (parser.IsExHeader())
			{
				if (packet_type == cmn::PacketType::SEQUENCE_HEADER)
				{
					if (video_data->avc_header == nullptr)
					{
						logte("AVC sequence header is not found");
						OV_ASSERT2(false);
						return false;
					}

					media_packet = CreateMediaPacket(
						video_data->avc_header_data,
						pts, dts,
						packet_type,
						true);
				}
				else
				{
					if (video_data->payload == nullptr)
					{
						logte("AVC payload is not found");
						return false;
					}

					media_packet = CreateMediaPacket(
						video_data->payload,
						pts, dts,
						packet_type,
						video_data->IsKeyFrame());
				}

				_media_packet_list.push_back(media_packet);
			}
			else
			{
				if (video_data->payload == nullptr)
				{
					OV_ASSERT2(false);
					return false;
				}

				_media_packet_list.push_back(CreateMediaPacket(
					video_data->payload,
					pts, dts,
					packet_type,
					video_data->IsKeyFrame()));
			}
		} while (false);

		return true;
	}

	std::shared_ptr<MediaTrack> RtmpAvcTrack::CreateMediaTrack(
		const modules::flv::ParserCommon &parser,
		const std::shared_ptr<const modules::flv::CommonData> &data) const
	{
		auto video_data = std::dynamic_pointer_cast<const modules::flv::VideoData>(data);

		if (video_data == nullptr)
		{
			OV_ASSERT2(false);
			return nullptr;
		}

		auto media_track = RtmpVideoTrack::CreateMediaTrack(parser, data);

		media_track->SetVideoTimestampScale(1.0);

		return media_track;
	}
}  // namespace pvd::rtmp
