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
		bool IsBypass() const
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
			RegisterValue<CondOptional>("Codec", &_codec, [this]() -> bool {
				// <Codec> is an option when _bypass is true
				return _bypass;
			});
			RegisterValue<Optional>("Scale", &_scale);
			RegisterValue<CondOptional>("Width", &_width, [this]() -> bool {
				// <Width> is an option when _bypass is true
				return _bypass;
			});
			RegisterValue<CondOptional>("Height", &_height, [this]() -> bool {
				// <Height> is an option when _bypass is true
				return _bypass;
			});
			RegisterValue<CondOptional>("Bitrate", &_bitrate, [this]() -> bool {
				// <Bitrate> is an option when _bypass is true
				return _bypass;
			});
			RegisterValue<CondOptional>("Framerate", &_framerate, [this]() -> bool {
				// <Framerate> is an option when _bypass is true
				return _bypass;
			});
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