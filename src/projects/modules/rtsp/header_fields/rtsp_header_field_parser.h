//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2021 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once
#include <base/ovlibrary/ovlibrary.h>
#include "rtsp_header_field.h"
#include "rtsp_header_session_field.h"

class RtspHeaderFieldParser
{
public:
	static std::shared_ptr<RtspHeaderField> Parse(const ov::String &message);
};
