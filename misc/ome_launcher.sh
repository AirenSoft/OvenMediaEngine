#!/bin/bash

PREFIX=/opt/ovenmediaengine

##########################################################################################
# Environment Variables
##########################################################################################
if [[ "$OSTYPE" == "darwin"* ]]; then
    NCPU=$(sysctl -n hw.ncpu)
    OSNAME=$(sw_vers -productName)
    OSVERSION=$(sw_vers -productVersion)
else
    NCPU=$(nproc)

    # CentOS, Fedora
    if [ -f /etc/redhat-release ]; then
        OSNAME=$(cat /etc/redhat-release |awk '{print $1}')
        OSVERSION=$(cat /etc/redhat-release |sed s/.*release\ // |sed s/\ .*// | cut -d"." -f1)
    # Ubuntu
    elif [ -f /etc/os-release ]; then
        OSNAME=$(cat /etc/os-release | grep "^NAME" | tr -d "\"" | cut -d"=" -f2)
        OSVERSION=$(cat /etc/os-release | grep ^VERSION= | tr -d "\"" | cut -d"=" -f2 | cut -d"." -f1 | awk '{print  $1}')
        OSMINORVERSION=$(cat /etc/os-release | grep ^VERSION= | tr -d "\"" | cut -d"=" -f2 | cut -d"." -f2 | awk '{print  $1}')
    fi
fi

##########################################################################################
# Preload the installed drivers
##########################################################################################
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
    LIBNVIDIA_ML_PATH=
    LIBCUDA_PATH=
    LIBNPPICC_PATH=
    LIBNPPIG_PATH=

    if [ "${OSNAME}" == "Ubuntu" ]; then
        LIBNVIDIA_ML_PATH=/lib/x86_64-linux-gnu/libnvidia-ml.so.1
        LIBCUDA_PATH=/lib/x86_64-linux-gnu/libcuda.so.1
        LIBNPPICC_PATH=/lib/x86_64-linux-gnu/libnppicc.so.10
        LIBNPPIG_PATH=/lib/x86_64-linux-gnu/libnppig.so.10       
    elif  [ "${OSNAME}" == "CentOS" ] || [ "${OSNAME}" == "Amazon Linux" ]; then
        LIBNVIDIA_ML_PATH=/usr/lib64/libnvidia-ml.so.1
        LIBCUDA_PATH=/usr/lib64/libcuda.so.1
        LIBNPPICC_PATH=/usr/lib64/libnppicc.so.10
        LIBNPPIG_PATH=/usr/lib64/libnppig.so.10           
    else
        return
    fi
       
    if [ -f $LIBNVIDIA_ML_PATH ] && [ -f $LIBCUDA_PATH ] && [ -f $LIBNPPICC_PATH ] && [ -f $LIBNPPIG_PATH ]; then
        export LD_PRELOAD=$LIBNVIDIA_ML_PATH:$LIBCUDA_PATH:$LIBNPPICC_PATH:$LIBNPPIG_PATH:$LD_PRELOAD
    fi
}

# Check the installed drivers and preload them.
check_xilinx_driver
check_nvidia_driver

# Dependency library path
export LD_LIBRARY_PATH=${PREFIX}/lib:${PREFIX}/lib/stubs:$LD_LIBRARY_PATH

# Run as daemon 
exec /usr/bin/OvenMediaEngine $1


