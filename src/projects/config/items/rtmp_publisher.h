//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "publisher.h"
#include "cross_domain.h"

namespace cfg
{
	struct RtmpPublisher : public Publisher
	{
		const std::vector<Url> &GetCrossDomains() const
		{
			return _cross_domain.GetUrls();
		}

		PublisherType GetType() const override
		{
			return PublisherType::Rtmp;
		}

	protected:
		void MakeParseList() const override
		{
			Publisher::MakeParseList();

			RegisterValue<Optional>("CrossDomain", &_cross_domain);
		}

		CrossDomain _cross_domain;
	};
}