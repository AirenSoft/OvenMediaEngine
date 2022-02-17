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
	_supported_qsv = false;
	_supported_cuda = false;
	_device_context = nullptr;
}

bool TranscodeGPU::Initialze()
{
	if(_initialzed)
		return true;
		
	Uninitialize();

	logtd("Trying to initialize a hardware accelerator");

	int ret = ::av_hwdevice_ctx_create(&_device_context, AV_HWDEVICE_TYPE_QSV, "/dev/dri/render128", NULL, 0);
	if (ret < 0)
	{
		av_buffer_unref(&_device_context);
		_device_context = nullptr;
		_supported_qsv = false;
	}
	else
	{
		_supported_qsv = true;
		_initialzed = true;
		auto constraints = av_hwdevice_get_hwframe_constraints(_device_context, nullptr);
		logti("Supported Intel QuickSync hardware accelerator. hw.pixfmt: %d, sw.pixfmt : %d",
			  *constraints->valid_hw_formats,
			  *constraints->valid_sw_formats);


		return true;
	}

	ret = ::av_hwdevice_ctx_create(&_device_context, AV_HWDEVICE_TYPE_CUDA, "/dev/dri/render128", NULL, 0);
	if (ret < 0)
	{
		av_buffer_unref(&_device_context);
		_device_context = nullptr;
	}
	else
	{
		_initialzed = true;
		_supported_cuda = true;
		auto constraints = av_hwdevice_get_hwframe_constraints(_device_context, nullptr);
		logti("Supported NVIDIA CUDA hardware accelerator. hw.pixfmt: %d, sw.pixfmt : %d",
			  *constraints->valid_hw_formats,
			  *constraints->valid_sw_formats);

		return true;
	}

	return false;
}

bool TranscodeGPU::Uninitialize()
{
	// Uninitialize device context of Intel Quicksync
	if (_device_context != nullptr)
	{
		av_buffer_unref(&_device_context);
		_device_context = nullptr;
	}

	return true;
}

AVBufferRef* TranscodeGPU::GetDeviceContext()
{
	return _device_context;
}


bool TranscodeGPU::IsSupportedQSV()
{
	return _supported_qsv;
}

bool TranscodeGPU::IsSupportedNV()
{
	return _supported_cuda;
}