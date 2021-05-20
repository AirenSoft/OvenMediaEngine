//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/ovlibrary/ovlibrary.h>

#include "codec/codec_base.h"

class TranscodeGPU : public ov::Singleton<TranscodeGPU>
{
public:
	TranscodeGPU();

	bool Initialze();
	bool Uninitialize();

	AVBufferRef *GetDeviceContext();
	bool IsSupportedQSV();
protected:
	AVBufferRef *_intel_quick_device_context;
};