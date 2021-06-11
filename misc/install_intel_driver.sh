#!/bin/bash
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
    CURRENT=$(pwd)

echo ${OSTYPE} / ${OSNAME} / ${OSVERSION}.${OSMINORVERSION}
echo ${CURRENT}
##########################################################################################

PREFIX=/opt/ovenmediaengine
TEMP_PATH=/tmp

LIBVA_VERSION=2.11.0
GMMLIB_VERSION=20.4.1
INTEL_MEDIA_DRIVER_VERSION=20.4.5
INTEL_MEDIA_SDK_VERSION=20.5.1

install_libva() {
    (DIR=${TEMP_PATH}/libva && \
    mkdir -p ${DIR} && \
    cd ${DIR} && \
    curl -sLf https://github.com/intel/libva/archive/refs/tags/${LIBVA_VERSION}.tar.gz | tar -xz --strip-components=1 && \
    ./autogen.sh --prefix="${PREFIX}" && \
    make -j$(nproc) && \
    sudo make install && \
    rm -rf ${DIR}) || fail_exit "libva"
}

install_gmmlib() {
    (DIR=${TEMP_PATH}/gmmlib && \
    mkdir -p ${DIR} && \
    cd ${DIR} && \
    curl -sLf https://github.com/intel/gmmlib/archive/refs/tags/intel-gmmlib-${GMMLIB_VERSION}.tar.gz | tar -xz --strip-components=1 && \
    mkdir -p ${DIR}/build && \
    cd ${DIR}/build && \
    cmake -DCMAKE_INSTALL_PREFIX="${PREFIX}" .. && \
    make -j$(nproc) && \
    sudo make install && \
    rm -rf ${DIR}) || fail_exit "gmmlib"
}

# Note: gmmlib must be pre-installed
# Note: gmmlib source is required to build the intel media driver
install_intel_media_driver() {
    (DIR_IMD=${TEMP_PATH}/media-driver && \
    mkdir -p ${DIR_IMD} && \
    cd ${DIR_IMD} && \
    curl -sLf https://github.com/intel/media-driver/archive/refs/tags/intel-media-${INTEL_MEDIA_DRIVER_VERSION}.tar.gz  | tar -xz --strip-components=1 && \
    DIR_GMMLIB=${TEMP_PATH}/gmmlib && \
    mkdir -p ${DIR_GMMLIB} && \
    cd ${DIR_GMMLIB} && \
    curl -sLf https://github.com/intel/gmmlib/archive/refs/tags/intel-gmmlib-${GMMLIB_VERSION}.tar.gz | tar -xz --strip-components=1 && \
    DIR=${TEMP_PATH}/build && \
    mkdir -p ${DIR} && \
    cd ${DIR} && \
    PKG_CONFIG_PATH=${PREFIX}/lib/pkgconfig:${PKG_CONFIG_PATH} cmake \
        $DIR_IMD \
        -DBUILD_TYPE=release \
        -DBS_DIR_GMMLIB="$DIR_GMMLIB/Source/GmmLib" \
        -DBS_DIR_COMMON=$DIR_GMMLIB/Source/Common \
        -DBS_DIR_INC=$DIR_GMMLIB/Source/inc \
        -DBS_DIR_MEDIA=$DIR_IMD \
        -DCMAKE_INSTALL_PREFIX=${PREFIX} \
        -DCMAKE_INSTALL_LIBDIR=${PREFIX}/lib \
        -DINSTALL_DRIVER_SYSCONF=OFF \
        -DLIBVA_DRIVERS_PATH=${PREFIX}/lib/dri && \
    sudo make -j$(nproc) install && \
    rm -rf ${DIR} && \
    rm -rf ${DIR_IMD} && \
    rm -rf ${DIR_GMMLIB}) || fail_exit "intel_media_driver"

    echo "export LIBVA_DRIVERS_PATH=${PREFIX}/lib"
    echo "export LIBVA_DRIVER_NAME=iHD"
}

install_intel_media_sdk() {
    (DIR=${TEMP_PATH}/medka-sdk && \
    mkdir -p ${DIR} && \
    cd ${DIR} && \
    curl -sLf https://github.com/Intel-Media-SDK/MediaSDK/archive/refs/tags/intel-mediasdk-${INTEL_MEDIA_SDK_VERSION}.tar.gz  | tar -xz --strip-components=1 && \
    mkdir -p ${DIR}/build && \
    cd ${DIR}/build && \
    PKG_CONFIG_PATH=${PREFIX}/lib/pkgconfig:${PKG_CONFIG_PATH} cmake -DCMAKE_INSTALL_PREFIX="${PREFIX}" .. && \
    make -j$(nproc) && \
    sudo make install && \
    rm -rf ${DIR}) || fail_exit "intel_media_sdk"
}


install_base_ubuntu()
{
    sudo apt install -y libdrm-dev xorg xorg-dev openbox libx11-dev libgl1-mesa-glx libgl1-mesa-dev

    echo "Reboot is required to use the new video driver."
}

install_base_centos()
{
    if [[ "${OSVERSION}" == "7" ]]; then

        # Centos 7 uses the 2.8.x version of cmake by default. It must be changed to version 3.x or higher.
        sudo yum remove -y cmake
        sudo yum install -y cmake3
        sudo ln -s /usr/bin/cmake3 /usr/bin/cmake

        sudo yum install -y libdrm-devel libX11-devel libXi-devel

    elif [[ "${OSVERSION}" == "8" ]]; then
        
        sudo yum install -y libdrm-devel libX11-devel libXi-devel

    else
        fail_exit
    fi
}

fail_exit()
{
    echo "($1) installation has failed."
    cd ${CURRENT}
    exit 1
}

check_version()
{
    if [[ "${OSNAME}" == "Ubuntu" && "${OSVERSION}" != "18" && "${OSVERSION}" != "20" ]]; then
        proceed_yn
    fi

    if [[ "${OSNAME}" == "CentOS" && "${OSVERSION}" != "7" && "${OSVERSION}" != "8" ]]; then
        proceed_yn
    fi
}

proceed_yn()
{
    read -p "This program [$0] is tested on [Ubuntu 18/20.04, CentOS 7/8 q, Fedora 28] Do you want to continue [y/N] ? " ANS
    if [[ "${ANS}" != "y" && "$ANS" != "yes" ]]; then
        cd ${CURRENT}
        exit 1
    fi
}

no_supported()
{
    echo "Nvidia driver and toolkit are not supported on this platform"
    exit 1
}

if [ "${OSNAME}" == "Ubuntu" ]; then
    check_version
    install_base_ubuntu
elif  [ "${OSNAME}" == "CentOS" ]; then
    check_version
    install_base_centos
elif  [ "${OSNAME}" == "AmazonLinux" ]; then
    check_version
    # TODO : For Cloud Instance
else
    echo "This program [$0] does not support your operating system [${OSNAME}]"
    echo "Please refer to manual installation page"
fi

