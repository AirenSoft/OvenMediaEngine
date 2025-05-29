//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2025 AirenSoft. All rights reserved.
//
//==============================================================================
#include "./rtmp_audio_track.h"

#include "../rtmp_provider_private.h"

namespace pvd::rtmp
{
	int SoundRateToNumber(std::optional<modules::flv::SoundRate> sound_rate)
	{
		if (sound_rate.has_value() == false)
		{
			return 0;
		}

		switch (sound_rate.value())
		{
			OV_CASE_RETURN(modules::flv::SoundRate::_5_5, 5500);
			OV_CASE_RETURN(modules::flv::SoundRate::_11, 11000);
			OV_CASE_RETURN(modules::flv::SoundRate::_22, 22000);
			OV_CASE_RETURN(modules::flv::SoundRate::_44, 44000);
		}

		OV_ASSERT(false, "Unknown SoundRate");
		return 0;
	}

	cmn::AudioSample::Format SoundSizeToFormat(std::optional<modules::flv::SoundSize> sound_size)
	{
		if (sound_size.has_value() == false)
		{
			return cmn::AudioSample::Format::S16;
		}


		OV_ASSERT(false, "Unknown SoundSize");
		return cmn::AudioSample::Format::None;
	}
}  // namespace pvd::rtmp
