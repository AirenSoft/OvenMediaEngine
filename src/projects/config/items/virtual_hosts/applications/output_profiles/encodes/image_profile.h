//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

namespace cfg
{
	namespace vhost
	{
		namespace app
		{
			namespace oprf
			{
				struct ImageProfile : public Item
				{
				protected:
					bool _active = true;
					ov::String _codec;
					ov::String _scale;
					int _width = 0;
					int _height = 0;
					double _framerate = 0.0;

				public:
					CFG_DECLARE_REF_GETTER_OF(IsActive, _active)
					CFG_DECLARE_REF_GETTER_OF(GetCodec, _codec)
					CFG_DECLARE_REF_GETTER_OF(GetScale, _scale)
					CFG_DECLARE_REF_GETTER_OF(GetWidth, _width)
					CFG_DECLARE_REF_GETTER_OF(GetHeight, _height)
					CFG_DECLARE_REF_GETTER_OF(GetFramerate, _framerate)

				protected:
					void MakeList() override
					{
						Register<Optional>("Active", &_active);
						Register<Optional>("Codec", &_codec);
						Register<Optional>("Scale", &_scale);
						Register<Optional>("Width", &_width);
						Register<Optional>("Height", &_height);
						Register<Optional>("Framerate", &_framerate);
					}
				};
			}  // namespace oprf
		}	   // namespace app
	}		   // namespace vhost
}  // namespace cfg