//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2025 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "./rtmp_video_track.h"

namespace pvd::rtmp
{
	class RtmpStreamV2;

	class RtmpAvcTrack : public RtmpVideoTrack
	{
	public:
		RtmpAvcTrack(
			std::shared_ptr<RtmpStreamV2> stream,
			uint32_t track_id,
			bool from_ex_header)
			: RtmpVideoTrack(std::move(stream), track_id, from_ex_header, cmn::MediaCodecId::H264, cmn::BitstreamFormat::H264_AVCC)
		{
		}

		//--------------------------------------------------------------------
		// Implementation of RtmpTrack
		//--------------------------------------------------------------------
		bool Handle(
			const std::shared_ptr<const modules::rtmp::Message> &message,
			const modules::flv::ParserCommon &parser,
			const std::shared_ptr<const modules::flv::CommonData> &data) override;

		std::shared_ptr<MediaTrack> CreateMediaTrack(
			const modules::flv::ParserCommon &parser,
			const std::shared_ptr<const modules::flv::CommonData> &data) const override;

	protected:
	};
}  // namespace pvd::rtmp
