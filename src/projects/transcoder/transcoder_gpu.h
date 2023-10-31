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

#define MAX_DEVICE_COUNT 16

class TranscodeGPU : public ov::Singleton<TranscodeGPU>
{
public:
	TranscodeGPU();

	bool Initialize();
	bool Uninitialize();

	AVBufferRef *GetDeviceContextQSV(int32_t gpu_id = 0);
	AVBufferRef *GetDeviceContextNV(int32_t gpu_id = 0);

	bool IsSupportedQSV();
	bool IsSupportedNV();
	bool IsSupportedXMA();

protected:
	bool CheckSupportedQSV();
	bool CheckSupportedNV();
	bool CheckSupportedXMA();

	bool _initialized = false;

	uint32_t _device_count_xma;

	AVBufferRef *_device_context_qsv[MAX_DEVICE_COUNT];
	uint32_t _device_count_qsv;

	AVBufferRef *_device_context_nv[MAX_DEVICE_COUNT];
	uint32_t _device_count_nv;

	void CodecThread();
	std::thread _thread;
};