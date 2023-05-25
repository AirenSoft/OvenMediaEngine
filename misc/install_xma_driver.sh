#!/bin/bash

# ** This script is only available for Ubuntu and Amazon Linux OS on AWS VT1 instance.

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
    # Ubuntu, Amazon
    elif [ -f /etc/os-release ]; then
        OSNAME=$(cat /etc/os-release | grep "^NAME" | tr -d "\"" | cut -d"=" -f2)
        OSVERSION=$(cat /etc/os-release | grep ^VERSION= | tr -d "\"" | cut -d"=" -f2 | cut -d"." -f1 | awk '{print  $1}')
        OSMINORVERSION=$(cat /etc/os-release | grep ^VERSION= | tr -d "\"" | cut -d"=" -f2 | cut -d"." -f2 | awk '{print  $1}')
    fi
fi

CURRENT=$(pwd)
TEMP_PATH=/tmp
XMA_VERSION=2.0

echo "##########################################################################################"
echo " Install Xilinx Video SDK on AWS"
echo "##########################################################################################"
echo ${OSTYPE} / ${OSNAME} / ${OSVERSION}.${OSMINORVERSION}
echo ${CURRENT}


install_xilinx_video_sdk()
{
    RELEASE_DIR=

    if [[ "${OSNAME}" == "Ubuntu" && "${OSVERSION}" == "18" ]]; then
        RELEASE_DIR=U30_Ubuntu_18.04_v2.0.1
    elif [[ "${OSNAME}" == "Ubuntu" && "${OSVERSION}" == "20" ]]; then
        RELEASE_DIR=U30_Ubuntu_20.04_v2.0.1
    elif [[ "${OSNAME}" == "Amazon Linux" && "${OSVERSION}" == "2" ]]; then
        RELEASE_DIR=U30_AmazonLinux_2_v2.0.1
    else 
        echo "This video sdk does not support your operating system [${OSNAME} ${OSVERSION}.${OSMINORVERSION}]"
        fail_exit "xilinx_video_sdk"    
    fi  

    (DIR=${TEMP_PATH}/xma && \
    mkdir -p ${DIR} && \
    cd ${DIR} && \
    git clone https://github.com/Xilinx/video-sdk -b v2.0 --depth 1 && \
    cd ./video-sdk/release/${RELEASE_DIR} && \
    sudo ./install -sw && \
    rm -rf ${DIR}) || fail_exit "xilinx_video_sdk"    
}

install_base_ubuntu()
{
    sudo apt update
    sudo apt install -y linux-modules-extra-$(uname -r)
    sudo apt install -y python
    sudo apt install -y libsdl2-dev
    sudo apt install -y git
}


install_base_amazonlinux()
{
    sudo amazon-linux-extras install epel -y
    sudo yum install -y kernel-devel-$(uname -r) kernel-headers-$(uname -r) boost-devel gcc-c++  
    sudo yum install -y git 
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

    if [[ "${OSNAME}" == "Amazon Linux" && "${OSVERSION}" != "2" ]]; then
        proceed_yn
    fi    
}

proceed_yn()
{
    read -p "This program [$0] is tested on [Ubuntu 18/20.04, Amazon Linux 2] Do you want to continue [y/N] ? " ANS
    if [[ "${ANS}" != "y" && "$ANS" != "yes" ]]; then
        cd ${CURRENT}
        exit 1
    fi
}

if [ "${OSNAME}" == "Ubuntu" ]; then
    check_version
    install_base_ubuntu
elif  [ "${OSNAME}" == "Amazon Linux" ]; then
    check_version
    install_base_amazonlinux
else
    echo "This program [$0] does not support your operating system [${OSNAME}]"
fi

install_xilinx_video_sdk

echo "##########################################################################################"
echo " Reboot is required to use the xilinx video driver. "
echo " To activate the driver, enter the cli command below. "
echo " "
echo " $ source /opt/xilinx/xcdr/setup.sh "
echo "##########################################################################################"

