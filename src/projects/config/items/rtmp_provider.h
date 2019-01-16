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
	enum class OverlapStreamProcessType
	{
		Reject = 0,
		Refresh,
	};

	struct RtmpProvider : public Provider
	{
		ProviderType GetType() const override
		{
			return ProviderType::Rtmp;
		}

		int GetPort() const
		{
			return _port;
		}

		OverlapStreamProcessType GetOverlapStreamProcessType() const
		{
			return _overlap_stream_process == "refresh" ? OverlapStreamProcessType::Refresh : OverlapStreamProcessType::Reject;
		}

	protected:
		void MakeParseList() const override
		{
			Provider::MakeParseList();

			RegisterValue<Optional>("Port", &_port);
			RegisterValue<Optional>("OverlapStreamProcess", &_overlap_stream_process);
		}

		int _port = 1935;
		ov::String _overlap_stream_process;
	};
}