//==============================================================================
//
//  OvenMediaEngine
//
//  Created by getroot
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include "base/ovlibrary/ovlibrary.h"

class SdpBase
{
public:
	virtual bool FromString(const ov::String &desc) = 0;

	bool Update()
	{
		return UpdateData(_sdp_text);
	}

	bool ToString(ov::String &sdp)
	{
		if(_sdp_text.IsEmpty())
		{
			if(Update() == false)
			{
				return false;
			}
		}

		sdp = _sdp_text;

		return true;
	}

protected:
	virtual bool UpdateData(ov::String &sdp) = 0;

private:
	ov::String _sdp_text;
};
