//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2023 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "provider.h"

namespace cfg
{
	namespace vhost
	{
		namespace app
		{
			namespace pvd
			{
				struct ScheduledProvider : public Provider
				{
				protected:
                    ov::String _root_dir;
                    ov::String _schedule_files;

				public:
					ProviderType GetType() const override
					{
						return ProviderType::Scheduled;
					}

                    CFG_DECLARE_CONST_REF_GETTER_OF(GetRootDir, _root_dir)
                    CFG_DECLARE_CONST_REF_GETTER_OF(GetScheduleFiles, _schedule_files)
					
				protected:
					void MakeList() override
					{
						Provider::MakeList();

                        Register("RootDir", &_root_dir);
                        Register("ScheduleFiles", &_schedule_files);
					}
				};
			}  // namespace pvd
		} // namespace app
	} // namespace vhost
}  // namespace cfg