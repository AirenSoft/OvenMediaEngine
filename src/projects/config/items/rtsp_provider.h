#pragma once

#include "provider.h"

namespace cfg
{
	struct RtspProvider : public Provider
	{
		ProviderType GetType() const override
		{
			return ProviderType::Rtsp;
		}

	protected:
		void MakeParseList() const override
		{
			Provider::MakeParseList();
		}
	};
}