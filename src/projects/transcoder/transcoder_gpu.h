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

	bool Initialize();
	bool Uninitialize();

	AVBufferRef *GetDeviceContextQSV();
	AVBufferRef *GetDeviceContextNV();

	bool IsSupportedQSV();
	bool IsSupportedNV();
	bool IsSupportedXMA();

protected:
	bool CheckSupportedQSV();
	bool CheckSupportedNV();
	bool CheckSupportedXMA();

	bool _initialized = false;
	bool _supported_qsv;
	bool _supported_cuda;
	bool _supported_xma;
	
	AVBufferRef *_device_context_qsv;
	AVBufferRef *_device_context_nv;
};