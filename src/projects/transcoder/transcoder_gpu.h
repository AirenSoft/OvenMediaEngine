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
	bool IsSupportedNV();

protected:
	bool _initialzed = false;
	bool _supported_qsv;
	bool _supported_cuda;

	AVBufferRef *_device_context;

};