#!/bin/bash

check_xilinx_driver() {
    if [[ -f /opt/xilinx/xcdr/setup.sh ]]; then
        LIBXRM_PATH=/opt/xilinx/xrm/lib/libxrm.so.1
        LIBXRT_CORE_PATH=/opt/xilinx/xrt/lib/libxrt_core.so.2
        LIBXRT_COREUTIL_PATH=/opt/xilinx/xrt/lib/libxrt_coreutil.so.2
        LIBXMA2API_PATH=/opt/xilinx/xrt/lib/libxma2api.so.2

        source /opt/xilinx/xcdr/setup.sh -f

        if [ -f $LIBXRM_PATH ] && [ -f $LIBXRT_CORE_PATH ] && [ -f $LIBXRT_COREUTIL_PATH ] && [ -f $LIBXMA2API_PATH ]; then
            export LD_PRELOAD=$LIBXRM_PATH:$LIBXRT_CORE_PATH:$LIBXRT_COREUTIL_PATH:$LIBXMA2API_PATH:$LD_PRELOAD
        fi
    fi
}

check_nvidia_driver() {
    LIBNVIDIA_ML_PATH=/lib/x86_64-linux-gnu/libnvidia-ml.so.1
    LIBCUDA_PATH=/lib/x86_64-linux-gnu/libcuda.so.1
    
    if [ -f $LIBNVIDIA_ML_PATH ] && [ -f $LIBCUDA_PATH ]; then
        export LD_PRELOAD=$LIBNVIDIA_ML_PATH:$LIBCUDA_PATH:$LD_PRELOAD
    fi
}

# Check the installed drivers and preload them.
check_xilinx_driver
check_nvidia_driver

# Dependency library path
export LD_LIBRARY_PATH=/opt/ovenmediaengine/lib:/opt/ovenmediaengine/lib/stubs:$LD_LIBRARY_PATH

# Run as daemon 
exec /usr/bin/OvenMediaEngine -d
