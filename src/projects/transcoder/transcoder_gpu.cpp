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

	// XMA
	if(CheckSupportedXMA() == true)
	{
		logti("Supported Xilinx Media Accelerator");
	}
	else
	{
		logtw("No supported Xilinx Media Accelerator");
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

	_initialized = true;

	// Resource Monitoring Thread
	_thread = std::thread(&TranscodeGPU::CodecThread, this);

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

#ifdef HWACCELS_NVIDIA_ENABLED	
	nvmlShutdown();
#endif
	return true;
}

bool TranscodeGPU::IsSupported(cmn::MediaCodecModuleId id, int32_t gpu_id)
{
	switch(id)
	{
		case cmn::MediaCodecModuleId::QSV:
			return IsSupportedQSV(gpu_id);
		case cmn::MediaCodecModuleId::NVENC:
			return IsSupportedNV(gpu_id);
		case cmn::MediaCodecModuleId::XMA:
			return IsSupportedXMA(gpu_id);
		default:
			break;
	}

	return false;
}

int32_t TranscodeGPU::GetDeviceCount(cmn::MediaCodecModuleId id)
{
	switch(id)
	{
		case cmn::MediaCodecModuleId::QSV:
			return GetDeviceCountQSV();
		case cmn::MediaCodecModuleId::NVENC:
			return GetDeviceCountNV();
		case cmn::MediaCodecModuleId::XMA:
			return GetDeviceCountXMA();
		default:
			break;
	}

	return 0;
}

AVBufferRef *TranscodeGPU::GetDeviceContext(cmn::MediaCodecModuleId id, int32_t gpu_id)
{
	switch(id)
	{
		case cmn::MediaCodecModuleId::QSV:
			return GetDeviceContextQSV(gpu_id);
		case cmn::MediaCodecModuleId::NVENC:
			return GetDeviceContextNV(gpu_id);
		case cmn::MediaCodecModuleId::XMA:
		default:
			break;
	}

	return nullptr;
}

int32_t TranscodeGPU::GetDeviceCountQSV()
{
	return _device_count_qsv;
}

int32_t TranscodeGPU::GetDeviceCountNV()
{
	return _device_count_nv;
}

int32_t TranscodeGPU::GetDeviceCountXMA()
{
	return _device_count_xma;
}

bool TranscodeGPU::CheckSupportedNV()
{
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
		return false;
	}

	_device_count_nv = 0;

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
		char device_name[NVML_DEVICE_NAME_BUFFER_SIZE];
		nvmlDeviceGetName(device, device_name, sizeof(device_name));

		// Get GPU PCI Bus ID
		nvmlPciInfo_t pci_info;
		nvmlDeviceGetPciInfo(device, &pci_info);

		// Get CUDA device index
		unsigned int cuda_index = -1;
		for (unsigned int j = 0; j < device_count; j++)
		{
			CUdevice cu_device;
			cuDeviceGet(&cu_device, j);

			int32_t cu_pci_bus_is;
			cuDeviceGetAttribute(&cu_pci_bus_is, CU_DEVICE_ATTRIBUTE_PCI_BUS_ID , cu_device);

			if(cu_pci_bus_is == (int32_t)pci_info.bus)
			{
				cuda_index = j;
				break;
			}
		}
		
		// Get HW device context
		char device_id[10];
		sprintf(device_id, "%d", cuda_index);
		int ret = ::av_hwdevice_ctx_create(&_device_context_nv[gpu_id], AV_HWDEVICE_TYPE_CUDA, device_id, nullptr, 1);
		if (ret < 0)
		{
			av_buffer_unref(&_device_context_nv[gpu_id]);
			_device_context_nv[gpu_id] = nullptr;

			logtw("No supported CUDA computing unit. [%s]", device_name);
		}
		else
		{
			logti("NVIDIA. DeviceId(%d), Name(%40s), BusId(%d), CudaId(%d)", gpu_id, device_name, pci_info.bus, cuda_index);			

			// auto constraints = av_hwdevice_get_hwframe_constraints(_device_context_nv[gpu_id], nullptr);			
			// logtd("constraints. hw.fmt(%d), sw.fmt(%d)", *constraints->valid_hw_formats,*constraints->valid_sw_formats);

			_device_count_nv++;			
		}		
	}
	

	if(_device_count_nv == 0)
	{
		return false;
	}

	return true;

#else
	_device_count_nv = 0;

	return false;
#endif
}

bool TranscodeGPU::CheckSupportedXMA()
{
#ifdef XMA_ENABLED	
	xrmContext *xrm_ctx = (xrmContext *)xrmCreateContext(XRM_API_VERSION_1);
	if (xrm_ctx == NULL)
	{
		logte("xrmCreateContext: Failed");
	}			

	if (xrmIsDaemonRunning(xrm_ctx) == false)
	{
		logte("XRM daemon : NOT running");
		return false;
	}
		
	if (xrmDestroyContext(xrm_ctx) != XRM_SUCCESS)
	{
		logte("xrmDestroyContext: Failed");
		return false;
	}

	int32_t dev_id = 0;
	bool dev_list[MAX_XLNX_DEVS];
	XmaXclbinParameter xclbin_nparam[MAX_XLNX_DEVS];
	memset(dev_list, false, MAX_XLNX_DEVS * sizeof(bool));

	_device_count_xma = 0;

	for (dev_id = 0; dev_id < xma_num_devices(); dev_id++)
	{
		xclbin_nparam[dev_id].device_id = dev_id;
		xclbin_nparam[dev_id].xclbin_name = XLNX_XCLBIN_PATH;
		_device_count_xma++;
	}

	// Initialize all devices
	if (xma_initialize(xclbin_nparam, _device_count_xma) != 0)
	{
		return false;
	}

	return true;
#else
	_device_count_xma = 0;

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

bool TranscodeGPU::IsSupportedQSV(int32_t gpu_id)
{
	if(_device_count_qsv == 0 || gpu_id >= _device_count_qsv)
	{
		return false;
	}

	return true;
}

bool TranscodeGPU::IsSupportedNV(int32_t gpu_id)
{
	if (_device_count_nv == 0 || gpu_id >= _device_count_nv)
	{
		return false;
	}

	return true;
}

bool TranscodeGPU::IsSupportedXMA(int32_t gpu_id)
{
	if (_device_count_xma == 0 || gpu_id >= _device_count_xma)
	{
		return false;
	}

	return true;	
}

uint32_t TranscodeGPU::GetUtilization(IPType type, cmn::MediaCodecModuleId id, int32_t gpu_id)
{
	switch (id)
	{
		case cmn::MediaCodecModuleId::XMA: {
			if (type == IPType::ENCODER)
			{
				return 0;
			}
			else if (type == IPType::DECODER)
			{
				return 0;
			}
			else if (type == IPType::SCALER)
			{
				return 0;
			}			
		}
		break;
		case cmn::MediaCodecModuleId::NVENC: {
			nvmlDevice_t device;
			nvmlReturn_t result = nvmlDeviceGetHandleByIndex(gpu_id, &device);
			if (result != NVML_SUCCESS)
			{
				logte("Failed to get device handle: %s", nvmlErrorString(result));
				return 0;
			}

			if (type == IPType::ENCODER)
			{
				uint32_t encUtilization;
				uint32_t encSamplingPeriod;
				nvmlDeviceGetEncoderUtilization(device, &encUtilization, &encSamplingPeriod);

				return encUtilization;
			}
			else if (type == IPType::DECODER)
			{
				uint32_t decUtilization;
				uint32_t decSamplingPeriod;
				nvmlDeviceGetDecoderUtilization(device, &decUtilization, &decSamplingPeriod);

				return decUtilization;
			}
			else if (type == IPType::SCALER)
			{
				nvmlUtilization_st device_utilization;
				nvmlDeviceGetUtilizationRates(device, &device_utilization);

				return device_utilization.gpu;
			}			
			// logti("GpuId(%d), Name(%40s), dec.utilization(%d%%), enc.utilization(%d%%)", gpu_id, name, decUtilization, encUtilization);
		}
		break;
		case cmn::MediaCodecModuleId::QSV: {
			if (type == IPType::ENCODER)
			{
				return 0;
			}
			else if (type == IPType::DECODER)
			{
				return 0;
			}
			else if (type == IPType::SCALER)
			{
				return 0;
			}			
		}
		break;
		default:
			break;
	}

	return 0;
}

void TranscodeGPU::CodecThread()
{
#if 0	
	logte("================================================================");
	logte(" Compute Unit Reserve & Release");
	logte("================================================================");
	logte("Devices Count: %d", xma_num_devices());

	xrmContext *xrm_ctx = (xrmContext *)xrmCreateCoentxt(XRM_API_VERSION_1);
	if (xrm_ctx == NULL)
	{
		logte("xrmCreateContext: Failed");
	}			

	if (xrmIsDaemonRunning(xrm_ctx) == true)
		logte("XRM daemon : running");
	else
	{
		logte("XRM daemon : NOT running");
		return;
	}

	xrmCuPoolPropertyV2 cu_pool_prop;
	int32_t ret = xrmCheckCuPoolAvailableNumV2(xrm_ctx, &cu_pool_prop);
	if ( ret < 0)
	{
		logte("xrmCheckCuPoolAvailableNum: %d", ret);
		return;
	}

	std::vector<std::string> cu_kernel_names;
	// cu_kernel_names.push_back("scaler");
	// cu_kernel_names.push_back("lookahead");
	cu_kernel_names.push_back("decoder");
	// cu_kernel_names.push_back("encoder");
	// cu_kernel_names.push_back("kernel_vcu_decoder");
	// cu_kernel_names.push_back("kernel_vcu_decoder");
	// cu_kernel_names.push_back("VCU");

	
	// for (dev_id = 0; dev_id < xma_num_devices(); dev_id++)
	// {

	// 	for (const auto &kernel_name : cu_kernel_names)
	// 	{
	// 		xrmCuPoolProperty
	// 		xrmCuProperty cu_prop;
	// 		xrmCuResource cu_res;
	// 		xrmCuStat cu_stat;

	// 		uint32_t available_num  =0;

	// 		strcpy(cu_prop.kernelName, kernel_name.c_str());
	// 		strcpy(cu_prop.kernelAlias, "");
	// 		cu_prop.devExcl = false;
	// 		cu_prop.requestLoad = 100; // 0~100%
	// 		cu_prop.poolId = 0;


							
	// 		logti("deviceId:%d, name:%s, available_num:%d", dev_id, kernel_name.c_str(), available_num);


	// 	}
	// }


	for (const auto &kernel_name : cu_kernel_names)
	{
		logte("----------------------------------------------------------------");

		xrmCuProperty cu_prop;
		xrmCuResource cu_res;
		xrmCuStat cu_stat;

		strcpy(cu_prop.kernelName, kernel_name.c_str());
		strcpy(cu_prop.kernelAlias, "");
		cu_prop.devExcl = false;
		cu_prop.requestLoad = 0; // 0~100%
		cu_prop.poolId = 0;

		int32_t ret = xrmCheckCuAvailableNum(xrm_ctx, &cu_prop);
		if (ret < 0) {
			logte("xrmCheckCuAvailableNum: Failed");
		} else {
			logte("AvailableNum: %d", ret);
		}
		uint32_t available_num = ret;

		ret = xrmCuAlloc(xrm_ctx, &cu_prop, &cu_res);
		if (ret != 0) {
			logte("xrmCuAlloc: Failed \n");
		} else {
			logte("cuId: %d", cu_res.cuId);
			// logte("xclbinFileName: %s", cu_res.xclbinFileName);
			logte("PluginFileName: %s", cu_res.kernelPluginFileName);
			logte("kernelName: %s", cu_res.kernelName);
			logte("instanceName: %s", cu_res.instanceName);
			logte("cuName: %s", cu_res.cuName);
			logte("kernelAlias: %s", cu_res.kernelAlias);
			logte("deviceId: %d", cu_res.deviceId);
			
			logte("channelId: %d", cu_res.channelId);
			logte("cuType: %d", cu_res.cuType);
			logte("baseAddr: 0x%lx", cu_res.baseAddr);
			// logte("membankId: %d", cu_res.membankId);
			// logte("membankType: %d", cu_res.membankType);
			// logte("membankSize: 0x%lx", cu_res.membankSize);
			// logte("membankBaseAddr: 0x%lx", cu_res.membankBaseAddr);
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
			// continue;
		}
		
		logte("IsBusy: %s", cu_stat.isBusy?"true":"false");
		logte("UsedLoad:  %d", cu_stat.usedLoad);

		logte("[%s] AvailableNum(%d)", kernel_name.c_str(), available_num);
	
	}

	if (xrmDestroyContext(xrm_ctx) != XRM_SUCCESS)
	{
		logte("xrmDestroyContext: Failed");
	}
#endif	

/////================================================================================================
#ifdef HWACCELS_NVIDIA_ENABLED	
	while(false)
	{
		for (int gpu_id = 0; gpu_id < GetDeviceCount(cmn::MediaCodecModuleId::NVENC); gpu_id++)
		{
			logti("[%d] %d%%, %d%%, %d%%",
				  gpu_id,
				  GetUtilization(IPType::DECODER, cmn::MediaCodecModuleId::NVENC, gpu_id),
				  GetUtilization(IPType::ENCODER, cmn::MediaCodecModuleId::NVENC, gpu_id),
				  GetUtilization(IPType::SCALER, cmn::MediaCodecModuleId::NVENC, gpu_id));
		}
		std::this_thread::sleep_for(std::chrono::seconds(5));
	}
#endif
}


#if 0 // Example code for NVML
	while (false)
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