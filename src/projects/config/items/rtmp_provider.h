//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "provider.h"

namespace cfg
{
	struct RtmpProvider : public Provider
	{
		ProviderType GetType() const override
		{
			return ProviderType::Rtmp;
		}

		bool IsBlockDuplicateStreamName () const
		{
			return _is_block_duplicate_stream_name;
		}

	protected:
		void MakeParseList() const override
		{
			Provider::MakeParseList();

            RegisterValue<Optional>("BlockDuplicateStreamName", &_is_block_duplicate_stream_name);
		}

		bool _is_block_duplicate_stream_name = true; // true - block   false - non block
	};
}