//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "dash_publisher.h"
#include "file_publisher.h"
#include "hls_publisher.h"
#include "ll_dash_publisher.h"
#include "ovt_publisher.h"
#include "mpegtspush_publisher.h"
#include "rtmppush_publisher.h"
#include "thumbnail_publisher.h"
#include "webrtc_publisher.h"

namespace cfg
{
	namespace vhost
	{
		namespace app
		{
			namespace pub
			{
				struct Publishers : public Item
				{
					std::vector<const Publisher *> GetPublisherList() const
					{
						return
						{
							&_mpegtspush_publisher,
							&_hls_publisher,
							&_dash_publisher,
							&_ll_dash_publisher,
							&_webrtc_publisher,
							&_ovt_publisher,
							&_file_publisher,
							&_rtmppush_publisher,
							&_thumbnail_publisher
						};
					}

					CFG_DECLARE_CONST_REF_GETTER_OF(GetAppWorkerCount, _app_worker_count)
					CFG_DECLARE_CONST_REF_GETTER_OF(GetStreamWorkerCount, _stream_worker_count)
					CFG_DECLARE_CONST_REF_GETTER_OF(GetMpegtsPushPublisher, _mpegtspush_publisher)
					CFG_DECLARE_CONST_REF_GETTER_OF(GetHlsPublisher, _hls_publisher)
					CFG_DECLARE_CONST_REF_GETTER_OF(GetDashPublisher, _dash_publisher)
					CFG_DECLARE_CONST_REF_GETTER_OF(GetLlDashPublisher, _ll_dash_publisher)
					CFG_DECLARE_CONST_REF_GETTER_OF(GetWebrtcPublisher, _webrtc_publisher)
					CFG_DECLARE_CONST_REF_GETTER_OF(GetOvtPublisher, _ovt_publisher)
					CFG_DECLARE_CONST_REF_GETTER_OF(GetFilePublisher, _file_publisher)
					CFG_DECLARE_CONST_REF_GETTER_OF(GetRtmpPushPublisher, _rtmppush_publisher)
					CFG_DECLARE_CONST_REF_GETTER_OF(GetThumbnailPublisher, _thumbnail_publisher)

				protected:
					void MakeList() override
					{
						Register<Optional>("AppWorkerCount", &_app_worker_count);
						Register<Optional>("StreamWorkerCount", &_stream_worker_count);

						Register<Optional>("MPEGTSPush", &_mpegtspush_publisher);
						Register<Optional>({"HLS", "hls"}, &_hls_publisher);
						Register<Optional>({"DASH", "dash"}, &_dash_publisher);
						Register<Optional>({"LLDASH", "llDash"}, &_ll_dash_publisher);
						Register<Optional>({"WebRTC", "webrtc"}, &_webrtc_publisher);
						Register<Optional>({"OVT", "ovt"}, &_ovt_publisher);
						Register<Optional>({"FILE", "file"}, &_file_publisher);
						Register<Optional>({"RTMPPush", "rtmpPush"}, &_rtmppush_publisher);
						Register<Optional>({"Thumbnail", "thumbnail"}, &_thumbnail_publisher);
					}

					int _app_worker_count = 1;
					int _stream_worker_count = 8;

					MpegtsPushPublisher _mpegtspush_publisher;
					RtmpPushPublisher _rtmppush_publisher;
					HlsPublisher _hls_publisher;
					DashPublisher _dash_publisher;
					LlDashPublisher _ll_dash_publisher;
					WebrtcPublisher _webrtc_publisher;
					OvtPublisher _ovt_publisher;
					FilePublisher _file_publisher;
					ThumbnailPublisher _thumbnail_publisher;
				};
			}  // namespace pub
		}	   // namespace app
	}		   // namespace vhost
}  // namespace cfg
