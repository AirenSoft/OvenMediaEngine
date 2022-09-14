//=============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2022 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

namespace cfg
{
	namespace vhost
	{
		namespace app
		{
			namespace pvd
			{
				struct HLSID3v2 : public Item
				{
				protected:
					ov::String _inject_to; // video, audio, both
					ov::String _frame_type; // TXXX, WXXX, PRIV, T???, W???
					ov::String _info;
					ov::String _data;

				public:
					CFG_DECLARE_CONST_REF_GETTER_OF(GetInjectTo, _inject_to);
					CFG_DECLARE_CONST_REF_GETTER_OF(GetFrameType, _frame_type);
					CFG_DECLARE_CONST_REF_GETTER_OF(GetInfo, _info);
					CFG_DECLARE_CONST_REF_GETTER_OF(GetData, _data);

				protected:
					void MakeList() override
					{
						Register("InjectTo", &_inject_to);
						Register("FrameType", &_frame_type);
						Register<Optional>("Info", &_info);
						Register("Data", &_data);
					}
				};
			}  // namespace pvd
		} // namespace app
	} // namespace vhost
}  // namespace cfg
