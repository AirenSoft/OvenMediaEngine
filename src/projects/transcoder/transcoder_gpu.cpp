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
#include "xrm.h"
#define MAX_XLNX_DEVS 128
#define XLNX_XCLBIN_PATH (char*)"/opt/xilinx/xcdr/xclbins/transcode.xclbin"
#define MAX_XLNX_DEVICES_PER_CMD 2
#endif

TranscodeGPU::TranscodeGPU()
{
	_supported_qsv = false;
	_supported_cuda = false;
	_supported_xma = false;
	_device_context_qsv = nullptr;
	_device_context_nv = nullptr;
}

bool TranscodeGPU::Initialize()
{
	if(_initialized)
		return true;
		
	Uninitialize();

	logti("Trying to check the hardware accelerator");

	// QSV
	CheckSupportedQSV();
	// CUDA
	CheckSupportedNV();
	// XMA
	CheckSupportedXMA();

	_initialized = true;

	return false;
}

bool TranscodeGPU::CheckSupportedQSV()
{
	int ret = ::av_hwdevice_ctx_create(&_device_context_qsv, AV_HWDEVICE_TYPE_QSV, nullptr, nullptr, 0);
	if (ret < 0)
	{
		av_buffer_unref(&_device_context_qsv);
		_device_context_qsv = nullptr;
		_supported_qsv = false;

		logtw("There are no supported Intel QuickSync Accelerator");
	}
	else
	{
		_supported_qsv = true;
		_initialized = true;
		auto constraints = av_hwdevice_get_hwframe_constraints(_device_context_qsv, nullptr);
		logti("Supported Intel QuickSync Accelerator");
		logtd("constraints. hw.fmt(%d), sw.fmt(%d)",
			  *constraints->valid_hw_formats,
			  *constraints->valid_sw_formats);
		return true;
	}

	return false;
}

bool TranscodeGPU::CheckSupportedNV()
{
	int ret = ::av_hwdevice_ctx_create(&_device_context_nv, AV_HWDEVICE_TYPE_CUDA, nullptr, nullptr, 0);
	if (ret < 0)
	{
		av_buffer_unref(&_device_context_nv);
		_device_context_nv = nullptr;

		logtw("There are no supported NVIDIA Accelerator");
	}
	else
	{
		_initialized = true;
		_supported_cuda = true;
		auto constraints = av_hwdevice_get_hwframe_constraints(_device_context_nv, nullptr);
		logti("Supported NVIDIA Accelerator");
		logtd("constraints. hw.fmt(%d), sw.fmt(%d)",
			  *constraints->valid_hw_formats,
			  *constraints->valid_sw_formats);			  

		return true;
	}

	return false;
}

bool TranscodeGPU::CheckSupportedXMA()
{
#if 0
	logte("================================================================");
	logte(" Compute Unit Reserve & Release");
	logte("================================================================");
	logte("Devices Count: %d", xma_num_devices());

	xrmContext *xrm_ctx = (xrmContext *)xrmCreateContext(XRM_API_VERSION_1);
	if (xrm_ctx == NULL)
	{
		logte("xrmCreateContext: Failed");
	}			

	if (xrmIsDaemonRunning(xrm_ctx) == true)
		logte("XRM daemon : running");
	else
		logte("XRM daemon : NOT running");

	std::vector<std::string> cu_kernel_names;
	cu_kernel_names.push_back("scaler");
	cu_kernel_names.push_back("decoder");
	cu_kernel_names.push_back("encoder");
	cu_kernel_names.push_back("lookahead");
	cu_kernel_names.push_back("lookahead");
	cu_kernel_names.push_back("kernel_vcu_decoder");
	cu_kernel_names.push_back("kernel_vcu_decoder");
	cu_kernel_names.push_back("VCU");

	for (const auto &kernel_name : cu_kernel_names)
	{
		xrmCuProperty cu_prop;
		xrmCuResource cu_res;
		xrmCuStat cu_stat;

		strcpy(cu_prop.kernelName, kernel_name.c_str());
		strcpy(cu_prop.kernelAlias, "");
		cu_prop.devExcl = false;
		cu_prop.requestLoad = 100; // 0~100%
		cu_prop.poolId = 0;

		int32_t ret = xrmCheckCuAvailableNum(xrm_ctx, &cu_prop);
		if (ret < 0) {
			logte("xrmCheckCuAvailableNum: Failed");
		} else {
			logte("AvailableNum: %d", ret);
		}

		ret = xrmCuAlloc(xrm_ctx, &cu_prop, &cu_res);
		if (ret != 0) {
			logte("xrmCuAlloc: Failed \n");
		} else {
			logte("xclbinFileName: %s", cu_res.xclbinFileName);
			logte("PluginFileName: %s", cu_res.kernelPluginFileName);
			logte("kernelName: %s", cu_res.kernelName);
			logte("istanceName: %s", cu_res.instanceName);
			logte("cuName: %s", cu_res.cuName);
			logte("kernelAlias: %s", cu_res.kernelAlias);
			logte("deviceId: %d", cu_res.deviceId);
			logte("cuId: %d", cu_res.cuId);
			logte("channelId: %d", cu_res.channelId);
			logte("cuType: %d", cu_res.cuType);
			logte("baseAddr: 0x%lx", cu_res.baseAddr);
			logte("membankId: %d", cu_res.membankId);
			logte("membankType: %d", cu_res.membankType);
			logte("membankSize: 0x%lx", cu_res.membankSize);
			logte("membankBaseAddr: 0x%lx", cu_res.membankBaseAddr);
			logte("allocServiceId: %lu", cu_res.allocServiceId);
			logte("poolId: %lu", cu_res.poolId);
			logte("channelLoad: %d", cu_res.channelLoad);
		}

		if (!xrmCuRelease(xrm_ctx, &cu_res))
		{
			logte("xrmCuRelease: Failed");
		}

		uint64_t max_capacity = xrmCuGetMaxCapacity(xrm_ctx, &cu_prop);
    	logte("Max Capacity: %lu", max_capacity);

		ret = xrmCuCheckStatus(xrm_ctx, &cu_res, &cu_stat);
		if (ret != 0)
		{
			logte("xrmCuCheckStatus: Failed");
			continue;
		}
		
		logte("IsBusy: %s", cu_stat.isBusy?"true":"false");
		logte("UsedLoad:  %d", cu_stat.usedLoad);

		logte("----------------------------------------------------------------");
	}

	if (xrmDestroyContext(xrm_ctx) != XRM_SUCCESS)
	{
		logte("xrmDestroyContext: Failed");
	}
#endif

#ifdef XMA_ENABLED	
	int32_t dev_id = 0;
	int32_t xlnx_num_devs = 0;
	bool dev_list[MAX_XLNX_DEVS];
	XmaXclbinParameter xclbin_nparam[MAX_XLNX_DEVS];
	memset(dev_list, false, MAX_XLNX_DEVS * sizeof(bool));

	for (dev_id = 0; dev_id < xma_num_devices(); dev_id++)
	{
		xclbin_nparam[xlnx_num_devs].device_id = dev_id;
		xclbin_nparam[xlnx_num_devs].xclbin_name = XLNX_XCLBIN_PATH;
		xlnx_num_devs++;
	}

	// Initialize all devices
	if (xma_initialize(xclbin_nparam, xlnx_num_devs) == 0)
	{
		logti("Supported Xilinx Media Accelerator");
		logtd("constraints. Devices(%d)",
			xlnx_num_devs);	

		_initialized = true;
		_supported_xma = true;
		return true;
	}
	else {
		logtw("There are no supported Xilinx Media Accelerator");
	}
#else
	logtw("There are no supported Xilinx Media Accelerator");
#endif

	return false;
}

bool TranscodeGPU::Uninitialize()
{
	// Uninitialize device context of Intel Quicksync & NVCODEC
	if (_device_context_qsv != nullptr)
	{
		av_buffer_unref(&_device_context_qsv);
		_device_context_qsv = nullptr;
	}

	if (_device_context_qsv != nullptr)
	{
		av_buffer_unref(&_device_context_qsv);
		_device_context_qsv = nullptr;
	}

	return true;
}

AVBufferRef* TranscodeGPU::GetDeviceContextQSV()
{
	return _device_context_qsv;
}

AVBufferRef* TranscodeGPU::GetDeviceContextNV()
{
	return _device_context_nv;
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
