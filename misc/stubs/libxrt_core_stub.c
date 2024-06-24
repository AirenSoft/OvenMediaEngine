#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

// #include "libxrm_stub.h"

#define nullptr ((void*)0)
typedef void* xrmContext;
typedef void* XmaXclbinParameter;
typedef void* xclDeviceHandle;
typedef void* xclBufferHandle;
struct xclBOProperties { int stub; };
enum xclVerbosityLevel { stub };

bool xrmIsDaemonRunning(xrmContext context)
{
	return false;
}

int32_t xma_initialize(XmaXclbinParameter *devXclbins, int32_t num_parms)
{
	return 0;
}

xrmContext xrmCreateContext(uint32_t xrmApiVersion)
{
	return nullptr;
}

int xclGetBOProperties(xclDeviceHandle handle, xclBufferHandle boHandle, struct xclBOProperties *properties)
{
	return 0;
}

size_t xclWriteBO(xclDeviceHandle handle, xclBufferHandle boHandle, const void *src, size_t size, size_t seek)
{
	return 0;
}

size_t xclReadBO(xclDeviceHandle handle, xclBufferHandle boHandle, void *dst, size_t size, size_t skip)
{
	return 0;
}

void xclFreeBO(xclDeviceHandle handle, xclBufferHandle boHandle)
{
}

int32_t xma_num_devices()
{
	return 0;
}

xclDeviceHandle xclOpen(unsigned int deviceIndex, const char* unused1, enum xclVerbosityLevel unused2)
{
	return nullptr;
}

int32_t xrmDestroyContext(xrmContext context)
{
	return 0;
}

xclBufferHandle xclAllocBO(xclDeviceHandle handle, size_t size,int unused, unsigned int flags)
{
	return nullptr;
}