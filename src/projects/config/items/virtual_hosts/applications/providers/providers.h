//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "mpegts_provider.h"
#include "ovt_provider.h"
#include "rtmp_provider.h"
#include "rtsp_provider.h"
#include "rtsp_pull_provider.h"

namespace cfg
{
	namespace vhost
	{
		namespace app
		{
			namespace pvd
			{
				struct Providers : public Item
				{
					std::vector<const Provider *> GetProviderList() const
					{
						return {
							&_rtmp_provider,
							&_rtsp_pull_provider,
							&_rtsp_provider,
							&_ovt_provider,
							&_mpegts_provider};
					}

					CFG_DECLARE_REF_GETTER_OF(GetRtmpProvider, _rtmp_provider)
					CFG_DECLARE_REF_GETTER_OF(GetRtspPullProvider, _rtsp_pull_provider)
					CFG_DECLARE_REF_GETTER_OF(GetRtspProvider, _rtsp_provider)
					CFG_DECLARE_REF_GETTER_OF(GetOvtProvider, _ovt_provider)
					CFG_DECLARE_REF_GETTER_OF(GetMpegtsProvider, _mpegts_provider)

				protected:
					void MakeList() override
					{
						Register<Optional>({"RTMP", "rtmp"}, &_rtmp_provider);
						Register<Optional>({"RTSPPull", "rtspPull"}, &_rtsp_pull_provider);
						Register<Optional>({"RTSP", "rtsp"}, &_rtsp_provider);
						Register<Optional>({"OVT", "ovt"}, &_ovt_provider);
						Register<Optional>({"MPEGTS", "mpegts"}, &_mpegts_provider);
					};

					RtmpProvider _rtmp_provider;
					RtspPullProvider _rtsp_pull_provider;
					RtspProvider _rtsp_provider;
					OvtProvider _ovt_provider;
					MpegtsProvider _mpegts_provider;
				};
			}  // namespace pvd
		}	   // namespace app
	}		   // namespace vhost
}  // namespace cfg