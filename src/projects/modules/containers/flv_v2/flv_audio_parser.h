//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2025 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "./flv_audio_data.h"

namespace modules
{
	namespace flv
	{
		class AudioParser : public ParserBase
		{
		public:
			bool Parse(ov::BitReader &reader) override;

		protected:
			// Parse legacy AudioTagHeader (AAC only)
			MAY_THROWS(BitReaderError)
			bool ParseLegacyAAC(ov::BitReader &reader, const std::shared_ptr<AudioData> &data);

			MAY_THROWS(BitReaderError)
			bool ProcessExAudioTagHeader(ov::BitReader &reader, SoundFormat sound_format, bool *process_audio_body);
		};
	}  // namespace flv
}  // namespace modules
