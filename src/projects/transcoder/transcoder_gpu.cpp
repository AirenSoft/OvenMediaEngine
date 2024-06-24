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

#ifdef HWACCELS_XMA_ENABLED
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
		_device_context_nilogan[i] = nullptr;
		_device_context_nv[i] = nullptr;
	}
}

bool TranscodeGPU::Initialize()
{
	if(_initialized)
	{
		return true;
	}
		
	Uninitialize();

	logti("Trying to check the hardware accelerator");

	// CUDA
	if(CheckSupportedNV() == true)
	{
		logti("Supported NVIDIA Accelerator. Number of devices(%d)", GetDeviceCount(cmn::MediaCodecModuleId::NVENC));
	}
	else
	{
		logtw("No supported NVIDIA Accelerator");		
	}

	// XMA
	if(CheckSupportedXMA() == true)
	{
		logti("Supported Xilinx Media Accelerator. Number of devices(%d)", GetDeviceCount(cmn::MediaCodecModuleId::XMA));
	}
	else
	{
		logtw("No supported Xilinx Media Accelerator");
	}

	// QSV
	if(CheckSupportedQSV() == true)
	{
		logti("Supported Intel QuickSync Accelerator. Number of devices(%d)", GetDeviceCount(cmn::MediaCodecModuleId::QSV));
	}
	else
	{
		logtw("No supported Intel QuickSync Accelerator");
	}

	// NILOGAN
	if(CheckSupportedNILOGAN() == true)
	{
		logti("Supported Netint VPU Accelerator. Number of devices(%d)", GetDeviceCount(cmn::MediaCodecModuleId::NILOGAN));
	}
	else
	{
		logtw("No supported Netint VPU Accelerator");
	}

	_initialized = true;

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

		if (_device_context_nilogan[i] != nullptr)
		{
			av_buffer_unref(&_device_context_nilogan[i]);
			_device_context_nilogan[i] = nullptr;
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
		case cmn::MediaCodecModuleId::NILOGAN:
			return IsSupportedNILOGAN(gpu_id);
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
		case cmn::MediaCodecModuleId::NILOGAN:
			return GetDeviceCountNILOGAN();
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
		case cmn::MediaCodecModuleId::NILOGAN:
			return GetDeviceContextNILOGAN(gpu_id);
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

int32_t TranscodeGPU::GetDeviceCountNILOGAN()
{
	return _device_count_nilogan;
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
		logtd("NVML: Driver is not loaded or installed");
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
			_device_count_nv++;			
			logti("NVIDIA. DeviceId(%d), Name(%s), BusId(%d), CudaId(%d)", gpu_id, device_name, pci_info.bus, cuda_index);			

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
#ifdef HWACCELS_XMA_ENABLED	
	xrmContext *xrm_ctx = (xrmContext *)xrmCreateContext(XRM_API_VERSION_1);
	if (xrm_ctx == NULL)
	{
		logtd("XMA: Driver is not loaded or installed");
		return false;
	}			

	if (xrmIsDaemonRunning(xrm_ctx) == false)
	{
		logtw("XMA: Daemon is not running");
		return false;
	}
		
	if (xrmDestroyContext(xrm_ctx) != XRM_SUCCESS)
	{
		logte("XMA: Failed to destory context");
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

		logti("XMA: deviceId(%d), xclbin(%s)", xclbin_nparam[dev_id].device_id, xclbin_nparam[dev_id].xclbin_name);			
	}

	// Initialize all devices
	if (xma_initialize(xclbin_nparam, _device_count_xma) != 0)
	{
		logte("XMA: Failed to initialze");
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

		logtd("QSV: Driver is not loaded or installed");

		return false;
	}
	
	_device_count_qsv++;

	[[maybe_unused]]
	auto constraints = av_hwdevice_get_hwframe_constraints(_device_context_qsv[0], nullptr);
	logtd("constraints. hw.fmt(%d), sw.fmt(%d)", *constraints->valid_hw_formats, *constraints->valid_sw_formats);

	return true;
}

bool TranscodeGPU::CheckSupportedNILOGAN()
{
	_device_count_nilogan = 0;
#ifdef HWACCELS_NILOGAN_ENABLED		
	int ret = ::av_hwdevice_ctx_create(&_device_context_nilogan[0], AV_HWDEVICE_TYPE_NI_LOGAN, nullptr, nullptr, 0);
	if (ret < 0)
	{
		av_buffer_unref(&_device_context_nilogan[0]);
		_device_context_nilogan[0] = nullptr;

		logtd("Netint: Driver is not loaded or installed");
		return false;
	}
	
	_device_count_nilogan++;		

	auto constraints = av_hwdevice_get_hwframe_constraints(_device_context_nilogan[0], nullptr);
	
	logtd("constraints. hw.fmt(%d), sw.fmt(%d)", *constraints->valid_hw_formats, *constraints->valid_sw_formats);

	return true;
#else
	return false;
#endif
}


AVBufferRef* TranscodeGPU::GetDeviceContextQSV(int32_t gpu_id)
{
	if (gpu_id >= MAX_DEVICE_COUNT)
	{
		return nullptr;
	}

	return _device_context_qsv[gpu_id];
}

AVBufferRef* TranscodeGPU::GetDeviceContextNILOGAN(int32_t gpu_id)
{
	if (gpu_id >= MAX_DEVICE_COUNT)
	{
		return nullptr;
	}

	return _device_context_nilogan[gpu_id];
}

AVBufferRef* TranscodeGPU::GetDeviceContextNV(int32_t gpu_id)
{
	if(gpu_id >= MAX_DEVICE_COUNT)
	{
		return nullptr;
	}

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

bool TranscodeGPU::IsSupportedNILOGAN(int32_t gpu_id)
{
	if(_device_count_nilogan == 0 || gpu_id >= _device_count_nilogan)
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
#ifdef HWACCELS_XMA_ENABLED				
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
#endif			
		}
		break;
		case cmn::MediaCodecModuleId::NVENC: {
#ifdef HWACCELS_NVIDIA_ENABLED				
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
#endif			
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
		case cmn::MediaCodecModuleId::NILOGAN: {
#ifdef HWACCELS_NILOGAN_ENABLED				
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
#endif				
		}
		break;
		default:
			break;
	}

	return 0;
}

void TranscodeGPU::CodecThread()
{
	std::vector<cmn::MediaCodecModuleId> modules;

	modules.push_back(cmn::MediaCodecModuleId::NVENC);
	modules.push_back(cmn::MediaCodecModuleId::XMA);
	modules.push_back(cmn::MediaCodecModuleId::QSV);	
	modules.push_back(cmn::MediaCodecModuleId::NILOGAN);	

	for(auto module : modules)
	{
		for (int gpu_id = 0; gpu_id < GetDeviceCount(module); gpu_id++)
		{
			logti("[%s:%d] %d%%, %d%%, %d%%",
				  cmn::GetStringFromCodecModuleId(module).CStr(),
				  gpu_id,
				  GetUtilization(IPType::DECODER, module, gpu_id),
				  GetUtilization(IPType::ENCODER, module, gpu_id),
				  GetUtilization(IPType::SCALER, module, gpu_id));
		}
	}
}
