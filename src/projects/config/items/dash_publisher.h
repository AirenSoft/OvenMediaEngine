//=============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "publisher.h"
#include "tls.h"
#include "cross_domain.h"

namespace cfg
{
	struct DashPublisher : public Publisher
	{
		PublisherType GetType() const override
		{
			return PublisherType::Dash;
		}

		int GetSegmentCount() const
		{
			return _segment_count;
		}

		int GetSegmentDuration() const
		{
			return _segment_duration;
		}

		const std::vector<Url> &GetCrossDomains() const
		{
			return _cross_domain.GetUrls();
		}

		int GetThreadCount() const
		{
			return _thread_count > 0 ? _thread_count : 1;
		}

		int GetSendBufferSize() const
		{
			return _send_buffer_size;
		}

		int GetRecvBufferSize() const
		{
			return _recv_buffer_size;
		}

	protected:
		void MakeParseList() const override
		{
			Publisher::MakeParseList();

			RegisterValue<Optional>("SegmentCount", &_segment_count);
			RegisterValue<Optional>("SegmentDuration", &_segment_duration);
			RegisterValue<Optional>("CrossDomain", &_cross_domain);
			RegisterValue<Optional>("ThreadCount", &_thread_count);
			RegisterValue<Optional>("SendBufferSize", &_send_buffer_size);
			RegisterValue<Optional>("RecvBufferSize", &_recv_buffer_size);
		}

		int _segment_count = 3;
		int _segment_duration = 5;
		CrossDomain _cross_domain;
		int _thread_count = 4;
		int _send_buffer_size = 1024 * 1024 * 20; // 20M
		int _recv_buffer_size = 0;
	};
}
