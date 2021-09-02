//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Keukhan Kwon
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#include "transcoder_gpu.h"

#include "transcoder_private.h"

TranscodeGPU::TranscodeGPU()
{
	_intel_quick_device_context = nullptr;
	_nvidia_cuda_device_context = nullptr;
}

bool TranscodeGPU::Initialze()
{
	Uninitialize();

	logtd("Trying to initialize a hardware accelerator");

	int ret = ::av_hwdevice_ctx_create(&_intel_quick_device_context, AV_HWDEVICE_TYPE_QSV, "/dev/dri/render128", NULL, 0);
	if (ret < 0)
	{
		_intel_quick_device_context = nullptr;
	}
	else
	{
		auto constraints = av_hwdevice_get_hwframe_constraints(_intel_quick_device_context, nullptr);
		logti("Supported Intel QuickSync hardware accelerator. hw.pixfmt: %d, sw.pixfmt : %d, resolution: %dx%d -%dx%d",
			  *constraints->valid_hw_formats,
			  *constraints->valid_sw_formats,
			  constraints->min_width,
			  constraints->min_height,
			  constraints->max_width,
			  constraints->max_height);
	}

	ret = ::av_hwdevice_ctx_create(&_nvidia_cuda_device_context, AV_HWDEVICE_TYPE_CUDA, "/dev/dri/render128", NULL, 0);
	if (ret < 0)
	{
		_nvidia_cuda_device_context = nullptr;
	}
	else
	{
		auto constraints = av_hwdevice_get_hwframe_constraints(_nvidia_cuda_device_context, nullptr);
		logti("Supported NVIDIA CUDA hardware accelerator. hw.pixfmt: %d, sw.pixfmt : %d, resolution: %dx%d -%dx%d",
			  *constraints->valid_hw_formats,
			  *constraints->valid_sw_formats,
			  constraints->min_width,
			  constraints->min_height,
			  constraints->max_width,
			  constraints->max_height);
	}

	if (_intel_quick_device_context == nullptr && _nvidia_cuda_device_context == nullptr)
	{
		logti("There is no supported hardware accelerator");
	}

	return true;
}

bool TranscodeGPU::Uninitialize()
{
	// Uninitialize device context of Intel Quicksync
	if (_intel_quick_device_context != nullptr)
	{
		av_buffer_unref(&_intel_quick_device_context);
		_intel_quick_device_context = nullptr;
	}

	if (_nvidia_cuda_device_context != nullptr)
	{
		av_buffer_unref(&_nvidia_cuda_device_context);
		_nvidia_cuda_device_context = nullptr;
	}

	return true;
}

AVBufferRef* TranscodeGPU::GetDeviceContext()
{
	return _intel_quick_device_context;
}

AVBufferRef* TranscodeGPU::GetDeviceContextNV()
{
	return _nvidia_cuda_device_context;
}

bool TranscodeGPU::IsSupportedQSV()
{
	return (_intel_quick_device_context != nullptr) ? true : false;
}

bool TranscodeGPU::IsSupportedNV()
{
	return (_nvidia_cuda_device_context != nullptr) ? true : false;
}