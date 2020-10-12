//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "cmaf_publisher.h"
#include "dash_publisher.h"
#include "hls_publisher.h"
#include "rtmp_publisher.h"
#include "webrtc_publisher.h"
#include "ovt_publisher.h"
#include "file_publisher.h"
#include "rtmppush_publisher.h"

namespace cfg
{
	struct Publishers : public Item
	{
		std::vector<const Publisher *> GetPublisherList() const
		{
			return {
				&_rtmp_publisher,
				&_hls_publisher,
				&_dash_publisher,
				&_ll_dash_publisher,
				&_webrtc_publisher,
				&_ovt_publisher,
				&_file_publisher};
		}

		CFG_DECLARE_GETTER_OF(GetThreadCount, _thread_count)
		CFG_DECLARE_REF_GETTER_OF(GetRtmpPublisher, _rtmp_publisher)
		CFG_DECLARE_REF_GETTER_OF(GetHlsPublisher, _hls_publisher)
		CFG_DECLARE_REF_GETTER_OF(GetDashPublisher, _dash_publisher)
		CFG_DECLARE_REF_GETTER_OF(GetLlDashPublisher, _ll_dash_publisher)
		CFG_DECLARE_REF_GETTER_OF(GetWebrtcPublisher, _webrtc_publisher)
		CFG_DECLARE_REF_GETTER_OF(GetOvtPublisher, _ovt_publisher)
		CFG_DECLARE_REF_GETTER_OF(GetFilePublisher, _file_publisher)
		CFG_DECLARE_REF_GETTER_OF(GetRtmpPushPublisher, _rtmppush_publisher)


	protected:
		void MakeParseList() override
		{
			RegisterValue<Optional>("ThreadCount", &_thread_count);

			RegisterValue<Optional>("RTMP", &_rtmp_publisher);
			RegisterValue<Optional>("HLS", &_hls_publisher);
			RegisterValue<Optional>("DASH", &_dash_publisher);
			RegisterValue<Optional>("LLDASH", &_ll_dash_publisher);
			RegisterValue<Optional>("WebRTC", &_webrtc_publisher);
			RegisterValue<Optional>("OVT", &_ovt_publisher);
			RegisterValue<Optional>("FILE", &_file_publisher);
			RegisterValue<Optional>("RTMPPush", &_rtmppush_publisher);
		}

		int _thread_count = 4;

		RtmpPublisher _rtmp_publisher;
		RtmpPushPublisher _rtmppush_publisher;
		HlsPublisher _hls_publisher;
		DashPublisher _dash_publisher;
		LlDashPublisher _ll_dash_publisher;
		WebrtcPublisher _webrtc_publisher;
		OvtPublisher _ovt_publisher;
		FilePublisher _file_publisher;
	};
}  // namespace cfg