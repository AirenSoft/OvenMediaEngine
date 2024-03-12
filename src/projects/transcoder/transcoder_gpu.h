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
#include <base/mediarouter/media_type.h>

#include "codec/codec_base.h"

#define MAX_DEVICE_COUNT 16


class TranscodeGPU : public ov::Singleton<TranscodeGPU>
{

public:
	TranscodeGPU();

	bool Initialize();
	bool Uninitialize();

	bool IsSupported(cmn::MediaCodecModuleId id, int32_t gpu_id = 0);

	int32_t GetDeviceCount(cmn::MediaCodecModuleId id);

	AVBufferRef *GetDeviceContext(cmn::MediaCodecModuleId id, int32_t gpu_id = 0);

	enum class IPType : int32_t
	{
		DECODER = 0,
		ENCODER,
		SCALER,
	};
	uint32_t GetUtilization(IPType type, cmn::MediaCodecModuleId id, int32_t gpu_id = 0);

protected:
	bool CheckSupportedQSV();
	bool CheckSupportedNILOGAN();
	bool CheckSupportedNV();
	bool CheckSupportedXMA();

	AVBufferRef *GetDeviceContextQSV(int32_t gpu_id = 0);
	AVBufferRef *GetDeviceContextNILOGAN(int32_t gpu_id = 0);
	AVBufferRef *GetDeviceContextNV(int32_t gpu_id = 0);

	bool IsSupportedQSV(int32_t gpu_id = 0);
	bool IsSupportedNILOGAN(int32_t gpu_id = 0);
	bool IsSupportedNV(int32_t gpu_id = 0);
	bool IsSupportedXMA(int32_t gpu_id = 0);

	int32_t GetDeviceCountQSV();
	int32_t GetDeviceCountNILOGAN();
	int32_t GetDeviceCountNV();
	int32_t GetDeviceCountXMA();

	bool _initialized = false;

	int32_t _device_count_xma;

	AVBufferRef *_device_context_qsv[MAX_DEVICE_COUNT];
	int32_t _device_count_qsv;

	AVBufferRef *_device_context_nilogan[MAX_DEVICE_COUNT];
	int32_t _device_count_nilogan;

	AVBufferRef *_device_context_nv[MAX_DEVICE_COUNT];
	int32_t _device_count_nv;

	void CodecThread();
	std::thread _thread;

public:
	std::mutex& GetDeviceMutex() {
		return _device_mutex;
	}
protected:
	// Global synchronization for specific hardware (Xilinx U30)
	std::mutex _device_mutex;
};