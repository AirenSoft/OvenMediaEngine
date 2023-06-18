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

#ifdef XMA_ENABLED
#include "xma.h"
#define MAX_XLNX_DEVS 128
#define XLNX_XCLBIN_PATH "/opt/xilinx/xcdr/xclbins/transcode.xclbin"
#define MAX_XLNX_DEVICES_PER_CMD 2
#endif

TranscodeGPU::TranscodeGPU()
{
	_supported_qsv = false;
	_supported_cuda = false;
	_supported_xma = false;
	_device_context = nullptr;
}

bool TranscodeGPU::Initialize()
{
	if(_initialized)
		return true;
		
	Uninitialize();

	logtd("Trying to initialize a hardware accelerator");

	// QSV
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
		_initialized = true;
		auto constraints = av_hwdevice_get_hwframe_constraints(_device_context, nullptr);
		logti("Supported Intel QuickSync hardware accelerator. hw.pixfmt: %d, sw.pixfmt : %d",
			  *constraints->valid_hw_formats,
			  *constraints->valid_sw_formats);


		return true;
	}

	// CUDA
	ret = ::av_hwdevice_ctx_create(&_device_context, AV_HWDEVICE_TYPE_CUDA, "/dev/dri/render128", NULL, 0);
	if (ret < 0)
	{
		av_buffer_unref(&_device_context);
		_device_context = nullptr;
	}
	else
	{
		_initialized = true;
		_supported_cuda = true;
		auto constraints = av_hwdevice_get_hwframe_constraints(_device_context, nullptr);
		logti("Supported NVIDIA CUDA hardware accelerator. hw.pixfmt: %d, sw.pixfmt : %d",
			  *constraints->valid_hw_formats,
			  *constraints->valid_sw_formats);

		return true;
	}

	// XMA
	// TODO(soulk) : Improved device resource allocation
#ifdef XMA_ENABLED
	{
		int dev_id = 0, xlnx_num_devs = 0;
		bool dev_list[MAX_XLNX_DEVS];
		XmaXclbinParameter xclbin_nparam[MAX_XLNX_DEVS];
		memset(dev_list, false, MAX_XLNX_DEVS * sizeof(bool));

		xclbin_nparam[xlnx_num_devs].device_id = dev_id;
		xclbin_nparam[xlnx_num_devs].xclbin_name = XLNX_XCLBIN_PATH;
		xlnx_num_devs++;

		xclbin_nparam[xlnx_num_devs].device_id = dev_id;
		xclbin_nparam[xlnx_num_devs].xclbin_name = XLNX_XCLBIN_PATH;
		xlnx_num_devs++;

		if (xma_initialize(xclbin_nparam, xlnx_num_devs) == 0)
		{
			logti("Supported Xilinx media accelerator. DeviecCount(%d)", xlnx_num_devs);
			_initialized = true;
			_supported_xma = true;
			return true;
		}
	}
#endif

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

bool TranscodeGPU::IsSupportedXMA()
{
	return _supported_xma;
}
