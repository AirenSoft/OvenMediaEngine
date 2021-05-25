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
}

bool TranscodeGPU::Initialze()
{
	Uninitialize();

	logtd("Trying to initialize a GPU accelerator engine");

	int ret = ::av_hwdevice_ctx_create(&_intel_quick_device_context, AV_HWDEVICE_TYPE_QSV, "/dev/dri/render128", NULL, 0);
	if (ret < 0)
	{
		// logtw("Does not support Intel QuickSync");
		_intel_quick_device_context = nullptr;
	}
	else
	{
		logti("Supported Intel QuickSync");
		auto constraints = av_hwdevice_get_hwframe_constraints(_intel_quick_device_context, nullptr);
		// logti("minimum : %dx%d -> maximum : %dx%d", constraints->min_width, constraints->min_height, constraints->max_width, constraints->max_height);
		logtd("valid hardware pixel format : %d", *constraints->valid_hw_formats);
		logtd("valid software pixel format : %d", *constraints->valid_sw_formats);
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

	return true;
}

AVBufferRef* TranscodeGPU::GetDeviceContext()
{
	return _intel_quick_device_context;
}

bool TranscodeGPU::IsSupportedQSV()
{
	return (_intel_quick_device_context != nullptr) ? true : false;
}
