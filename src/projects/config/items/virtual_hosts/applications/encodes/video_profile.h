//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

namespace cfg
{
	struct VideoProfile : public Item
	{
		CFG_DECLARE_GETTER_OF(IsBypass, _bypass)
		CFG_DECLARE_GETTER_OF(IsActive, _active)
		CFG_DECLARE_GETTER_OF(GetHWAcceleration, _hw_acceleration)
		CFG_DECLARE_GETTER_OF(GetCodec, _codec)
		CFG_DECLARE_GETTER_OF(GetScale, _scale)
		CFG_DECLARE_GETTER_OF(GetWidth, _width)
		CFG_DECLARE_GETTER_OF(GetHeight, _height)
		CFG_DECLARE_GETTER_OF(GetBitrate, _bitrate)
		CFG_DECLARE_GETTER_OF(GetFramerate, _framerate)

	protected:
		void MakeParseList() override
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
}  // namespace cfg