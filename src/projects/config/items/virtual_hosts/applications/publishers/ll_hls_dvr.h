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
			namespace pub
			{
				struct LLHlsDvr : public Item
				{
				protected:
					bool _enabled = false;
					ov::String _temp_storage_path = "/tmp/ll_hls_dvr";
					int _max_duration = 3600;

				public:
					CFG_DECLARE_CONST_REF_GETTER_OF(IsEnabled, _enabled)
					CFG_DECLARE_CONST_REF_GETTER_OF(GetTempStoragePath, _temp_storage_path)
					CFG_DECLARE_CONST_REF_GETTER_OF(GetMaxDuration, _max_duration)

				protected:
					void MakeList() override
					{
						Register<Optional>("Enable", &_enabled);
						Register<Optional>("TempStoragePath", &_temp_storage_path);
						Register<Optional>("MaxDuration", &_max_duration);
						
					}
				};
			}  // namespace pub
		} // namespace app
	} // namespace vhost
}  // namespace cfg
