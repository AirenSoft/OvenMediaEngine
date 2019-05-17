//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "rtmp_publisher.h"
#include "hls_publisher.h"
#include "dash_publisher.h"
#include "webrtc_publisher.h"

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
				&_webrtc_publisher
			};
		}

		int GetThreadCount() const
		{
			return _thread_count;
		}

		const RtmpPublisher &GetRtmpPublisher() const
		{
			return _rtmp_publisher;
		}

		const HlsPublisher &GetHlsPublisher() const
		{
			return _hls_publisher;
		}

		const DashPublisher &GetDashPublisher() const
		{
			return _dash_publisher;
		}

		const WebrtcPublisher &GetWebrtcPublisher() const
		{
			return _webrtc_publisher;
		}

	protected:
		void MakeParseList() const override
		{
			RegisterValue<Optional>("ThreadCount", &_thread_count);

			RegisterValue<Optional>("RTMP", &_rtmp_publisher);
			RegisterValue<Optional>("HLS", &_hls_publisher);
			RegisterValue<Optional>("DASH", &_dash_publisher);
			RegisterValue<Optional>("WebRTC", &_webrtc_publisher);
		}

		int _thread_count;

		RtmpPublisher _rtmp_publisher;
		HlsPublisher _hls_publisher;
		DashPublisher _dash_publisher;
		WebrtcPublisher _webrtc_publisher;
	};
}