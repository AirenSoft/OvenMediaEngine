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

#ifdef HWACCELS_NVIDIA_ENABLED
#include <nvml.h>
#include <cuda.h>
#endif

TranscodeGPU::TranscodeGPU()
{
	for (int i = 0; i < MAX_DEVICE_COUNT; i++)
	{
		_device_context_qsv[i] = nullptr;
		_device_context_nv[i] = nullptr;
	}
}

bool TranscodeGPU::Initialize()
{
	if(_initialized)
		return true;
		
	Uninitialize();

	logti("Trying to check the hardware accelerator");

	// CUDA
	if(CheckSupportedNV() == true)
	{
		logti("Supported NVIDIA Accelerator");
	}
	else
	{
		logtw("No supported NVIDIA Accelerator");		
	}

	// QSV
	if(CheckSupportedQSV() == true)
	{
		logti("Supported Intel QuickSync Accelerator");
	}
	else
	{
		logtw("No supported Intel QuickSync Accelerator");
	}

	// XMA
	if(CheckSupportedXMA() == true)
	{
		logti("Supported Xilinx Media Accelerator");
	}
	else
	{
		logtw("No supported Xilinx Media Accelerator");
	}

	_initialized = true;

	// Resource Monitoring Thread
	// _thread = std::thread(&TranscodeGPU::CodecThread, this);

	return false;
}


bool TranscodeGPU::Uninitialize()
{
	for (int i = 0; i < MAX_DEVICE_COUNT; i++)
	{
		if (_device_context_nv[i] != nullptr)
		{
			av_buffer_unref(&_device_context_nv[i]);
			_device_context_nv[i] = nullptr;
		}

		if (_device_context_qsv[i] != nullptr)
		{
			av_buffer_unref(&_device_context_qsv[i]);
			_device_context_qsv[i] = nullptr;
		}
	}

	return true;
}


bool TranscodeGPU::CheckSupportedNV()
{
	_device_count_nv = 0;

#ifdef HWACCELS_NVIDIA_ENABLED	
	// Initialize NVML library
	nvmlReturn_t result = nvmlInit();
	if (result != NVML_SUCCESS)
	{
		logte("Failed to initialize NVML: %s", nvmlErrorString(result));
		return false;
	}

	// Initialize CUDA library
	if(cuInit(0) != CUDA_SUCCESS)
	{
		logte("Failed to initialize CUDA");
		return false;
	}
	
	// Get GPU device count
	unsigned int device_count;
	result = nvmlDeviceGetCount(&device_count);
	if (result != NVML_SUCCESS)
	{
		logte("Failed to get device count: %s", nvmlErrorString(result));
		nvmlShutdown();

		return false;
	}

	// Get GPU device handle
	for (unsigned int gpu_id = 0; gpu_id < device_count; gpu_id++)
	{
		nvmlDevice_t device;
		result = nvmlDeviceGetHandleByIndex(gpu_id, &device);
		if (result != NVML_SUCCESS)
		{
			logte("Failed to get device handle: %s", nvmlErrorString(result));
			
			continue;
		}

		// Get GPU name
		char name[NVML_DEVICE_NAME_BUFFER_SIZE];
		nvmlDeviceGetName(device, name, sizeof(name));

		// Get GPU PCI Bus ID
		nvmlPciInfo_t pciInfo;
		nvmlDeviceGetPciInfo(device, &pciInfo);

		// Get CUDA device index
		unsigned int cuda_index = -1;
		for (unsigned int j = 0; j < device_count; j++)
		{
			CUdevice cuDevice;
			cuDeviceGet(&cuDevice, j);

			int32_t cuPciBusId;
			cuDeviceGetAttribute(&cuPciBusId, CU_DEVICE_ATTRIBUTE_PCI_BUS_ID , cuDevice);

			if(cuPciBusId == (int32_t)pciInfo.bus)
			{
				cuda_index = j;
				break;
			}
		}
		
		char device_id[10];
		sprintf(device_id, "%d", cuda_index);
		int ret = ::av_hwdevice_ctx_create(&_device_context_nv[gpu_id], AV_HWDEVICE_TYPE_CUDA, device_id, nullptr, 1);
		if (ret < 0)
		{
			av_buffer_unref(&_device_context_nv[gpu_id]);
			_device_context_nv[gpu_id] = nullptr;

			logtw("No supported CUDA computing unit. [%s]", name);
		}
		else
		{
			auto constraints = av_hwdevice_get_hwframe_constraints(_device_context_nv[gpu_id], nullptr);

			logti("GpuId(%d), Name(%s), BusID(%d), CudaId(%d)", gpu_id, name, pciInfo.bus, cuda_index);			
			logtd("constraints. hw.fmt(%d), sw.fmt(%d)", *constraints->valid_hw_formats,*constraints->valid_sw_formats);

			_device_count_nv++;			
		}		
	}
	
	nvmlShutdown();

	if(_device_count_nv == 0)
	{
		return false;
	}

	return true;

#else
	return false;
#endif
}
bool TranscodeGPU::CheckSupportedQSV()
{
	_device_count_qsv = 0;

	int ret = ::av_hwdevice_ctx_create(&_device_context_qsv[0], AV_HWDEVICE_TYPE_QSV, nullptr, nullptr, 0);
	if (ret < 0)
	{
		av_buffer_unref(&_device_context_qsv[0]);
		_device_context_qsv[0] = nullptr;
		return false;
	}
	
	_device_count_qsv++;

	auto constraints = av_hwdevice_get_hwframe_constraints(_device_context_qsv[0], nullptr);
	logtd("constraints. hw.fmt(%d), sw.fmt(%d)", *constraints->valid_hw_formats, *constraints->valid_sw_formats);

	return true;
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
	
	_device_count_xma = 0;

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
		_device_count_xma++;
	}

	// Initialize all devices
	if (xma_initialize(xclbin_nparam, xlnx_num_devs) ! 0)
	{
		return false;
	}

	return true;
#else
	return false;
#endif
}

AVBufferRef* TranscodeGPU::GetDeviceContextQSV(int32_t gpu_id)
{
	if (gpu_id >= MAX_DEVICE_COUNT)
		return nullptr;

	return _device_context_qsv[gpu_id];
}

AVBufferRef* TranscodeGPU::GetDeviceContextNV(int32_t gpu_id)
{
	if(gpu_id >= MAX_DEVICE_COUNT)
		return nullptr;

	return _device_context_nv[gpu_id];
}

bool TranscodeGPU::IsSupportedQSV()
{
	return (_device_count_qsv>0)?true:false;
}

bool TranscodeGPU::IsSupportedNV()
{
	return (_device_count_nv>0)?true:false;
}

bool TranscodeGPU::IsSupportedXMA()
{
	return (_device_count_xma>0)?true:false;
}

void TranscodeGPU::CodecThread()
{
#ifdef HWACCELS_NVIDIA_ENABLED	
	while (true)
	{
		nvmlReturn_t result;

		result = nvmlInit();
		if (result != NVML_SUCCESS)
		{
			std::cerr << "Failed to initialize NVML: " << nvmlErrorString(result) << std::endl;
			return;
		}

		unsigned int device_count;
		result = nvmlDeviceGetCount(&device_count);
		if (result != NVML_SUCCESS)
		{
			std::cerr << "Failed to get device count: " << nvmlErrorString(result) << std::endl;
			nvmlShutdown();
			return;
		}

		for (unsigned int i = 0; i < device_count; i++)
		{
			nvmlDevice_t device;
			result = nvmlDeviceGetHandleByIndex(i, &device);
			if (result != NVML_SUCCESS)
			{
				std::cerr << "Failed to get device handle: " << nvmlErrorString(result) << std::endl;
				continue;
			}

			// GPU 이름 조회
			char name[NVML_DEVICE_NAME_BUFFER_SIZE];
			nvmlDeviceGetName(device, name, sizeof(name));

			// NVENC 사용량 조회
			nvmlEncoderType_t encoderQueryType = NVML_ENCODER_QUERY_H264;
			unsigned int encCapacity;
			result = nvmlDeviceGetEncoderCapacity(device, encoderQueryType, &encCapacity);
			if (result != NVML_SUCCESS)
			{
				std::cerr << "Failed to get encoder counts: " << nvmlErrorString(result) << std::endl;
				continue;
			}
			unsigned int encSessionCnt;
			unsigned int encAvrgFps;
			unsigned int encAvrgLatency;
			nvmlDeviceGetEncoderStats(device, &encSessionCnt, &encAvrgFps, &encAvrgLatency);


			unsigned int encUtilization;
			unsigned int encSamplingPeriod;
			nvmlDeviceGetEncoderUtilization(device, &encUtilization, &encSamplingPeriod);

			unsigned int decUtilization;
			unsigned int decSamplingPeriod;
			nvmlDeviceGetDecoderUtilization(device, &decUtilization, &decSamplingPeriod);

			unsigned int pciTxBytes, pciRxBytes;
			nvmlDeviceGetPcieThroughput(device, NVML_PCIE_UTIL_TX_BYTES, &pciTxBytes);
			nvmlDeviceGetPcieThroughput(device, NVML_PCIE_UTIL_RX_BYTES, &pciRxBytes);

			// nvmlReturn_t DECLDIR nvmlDeviceGetDecoderUtilization(nvmlDevice_t device, unsigned int *utilization, unsigned int *encSamplingPeriod);

			nvmlPciInfo_t pciInfo;
			nvmlDeviceGetPciInfo(device, &pciInfo);

			fprintf(stdout, "nvml[%d][%-40s][BudID:%d] dec.utilization:%d%%, enc.session:%d enc.avgfps:%d enc.latency:%d enc.utilization:%d%%, enc.capcity:%d, PCI Tx:%.2fMB/Rx:%.2fMB \n",
				  i, name, pciInfo.bus, decUtilization, encSessionCnt, encAvrgFps, encAvrgLatency, encUtilization, encCapacity, (double)pciTxBytes/1000000*50, (double)pciRxBytes/1000000*50);
		}

		// NVML 종료
		nvmlShutdown();

		// 1초간 슬립
		std::this_thread::sleep_for(std::chrono::seconds(10));
	}
#endif

}
