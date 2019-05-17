//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "../item.h"

namespace cfg
{
	struct VideoProfile : public Item
	{
		bool GetBypass() const
		{
			return _bypass;
		}

		bool IsActive() const
		{
			return _active;
		}

		ov::String GetHWAcceleration() const
		{
			return _hw_acceleration;
		}

		ov::String GetCodec() const
		{
			return _codec;
		}

		ov::String GetScale() const
		{
			return _scale;
		}

		int GetWidth() const
		{
			return _width;
		}

		int GetHeight() const
		{
			return _height;
		}

		ov::String GetBitrate() const
		{
			return _bitrate;
		}

		float GetFramerate() const
		{
			return _framerate;
		}

	protected:
		void MakeParseList() const override
		{
			RegisterValue<Optional>("Bypass", &_bypass);

			RegisterValue<Optional>("Active", &_active);
			RegisterValue<Optional>("HWAcceleration", &_hw_acceleration);
			RegisterValue("Codec", &_codec);
			RegisterValue<Optional>("Scale", &_scale);
			RegisterValue("Width", &_width);
			RegisterValue("Height", &_height);
			RegisterValue("Bitrate", &_bitrate);
			RegisterValue("Framerate", &_framerate);
		}

		bool _bypass = false;
		bool _active = true;
		ov::String _hw_acceleration = "none";
		ov::String _codec;
		ov::String _scale;
		int _width = 0;
		int _height = 0;
		ov::String _bitrate;
		float _framerate = 0.0f;
	};
}