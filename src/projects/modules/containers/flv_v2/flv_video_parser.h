//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2025 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "./flv_video_data.h"

namespace modules
{
	namespace flv
	{
		class VideoParser : public ParserBase
		{
		public:
			bool Parse(ov::BitReader &reader) override;

		protected:
			// Return `process_video_body` if the video body should be processed
			MAY_THROWS(BitReaderError)
			std::shared_ptr<VideoData> ProcessExVideoTagHeader(ov::BitReader &reader, VideoFrameType frame_type, bool *process_video_body);

			MAY_THROWS(BitReaderError)
			std::optional<rtmp::AmfDocument> ParseVideoMetadata(ov::BitReader &reader);

			MAY_THROWS(BitReaderError)
			bool ParseLegacyAvc(ov::BitReader &reader, const std::shared_ptr<VideoData> &data);

			MAY_THROWS(BitReaderError)
			std::shared_ptr<HEVCDecoderConfigurationRecord> ParseHEVC(ov::BitReader &reader);

			MAY_THROWS(BitReaderError)
			std::shared_ptr<VideoData> ProcessExVideoTagBody(ov::BitReader &reader, bool process_video_body, std::shared_ptr<VideoData> data);
		};
	}  // namespace flv
}  // namespace modules
