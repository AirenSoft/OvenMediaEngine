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
XMA_VERSION=3.0

echo "##########################################################################################"
echo " Install Xilinx Video SDK on AWS"
echo "##########################################################################################"
echo ${OSTYPE} / ${OSNAME} / ${OSVERSION}.${OSMINORVERSION}
echo ${CURRENT}


install_base_ubuntu()
{
    # Added resositroty
    # https://xilinx.github.io/video-sdk/v3.0/package_feed.html    
    sudo cp xilinx.plist /etc/apt/sources.list.d/

    # Remove older versions of the Xilinx Video SDK
    sudo apt-get remove xvbm xilinx-u30-xvbm xrmu30decoder xrmu30scaler xrmu30encoder xmpsoccodecs xmultiscaler xlookahead xmaapps xmapropstojson xffmpeg launcher jobslotreservation xcdr
    sudo apt-get remove xrm xilinx-container-runtime xilinx-xvbm xilinx-u30-xrm-decoder xilinx-u30-xrm-encoder xilinx-u30-xrm-multiscaler xilinx-u30-xma-multiscaler xilinx-u30-xlookahead xilinx-u30-xmpsoccodecs xilinx-u30-xma-apps xilinx-u30-xmapropstojson xilinx-u30-xffmpeg xilinx-u30-launcher xilinx-u30-jobslotreservation xilinx-u30-xcdr xilinx-u30-gstreamer-1.16.2 xilinx-u30-vvas xilinx-sc-fw-u30 xilinx-u30-gen3x4-base xilinx-u30-gen3x4-validate

    sudo apt-get -y update
    sudo apt-get -y install xrt=2.11.722
    sudo apt-mark hold xrt
    sudo apt-get -y install xilinx-alveo-u30-core    

    sudo cp /opt/xilinx/xcdr/xclbins/transcode.xclbin /opt/xilinx/xcdr/xclbins/transcode_lite.xclbin
    sudo cp /opt/xilinx/xcdr/xclbins/on_prem/transcode.xclbin /opt/xilinx/xcdr/xclbins/transcode.xclbin    
}


install_base_amazonlinux()
{
    # Added resositroty
    # https://xilinx.github.io/video-sdk/v3.0/package_feed.html
    sudo cp xilinx.repo /etc/yum.repos.d/

    # Install Required packages
    sudo amazon-linux-extras install epel -y

    # Remove older versions of the Xilinx Video SDK
    sudo yum -y remove xvbm xilinx-u30-xvbm xrmu30decoder xrmu30scaler xrmu30encoder xmpsoccodecs xmultiscaler xlookahead xmaapps xmapropstojson xffmpeg launcher jobslotreservation xcdr
    sudo yum -y remove xrm xilinx-container-runtime xilinx-xvbm xilinx-u30-xrm-decoder xilinx-u30-xrm-encoder xilinx-u30-xrm-multiscaler xilinx-u30-xma-multiscaler xilinx-u30-xlookahead xilinx-u30-xmpsoccodecs xilinx-u30-xma-apps xilinx-u30-xmapropstojson xilinx-u30-xffmpeg xilinx-u30-launcher xilinx-u30-jobslotreservation xilinx-u30-xcdr xilinx-u30-gstreamer-1.16.2 xilinx-u30-vvas xilinx-sc-fw-u30 xilinx-u30-gen3x4-base xilinx-u30-gen3x4-validate

    sudo yum -y update
    sudo yum -y install yum-plugin-versionlock
    sudo yum -y install xrt-2.11.722-1.x86_64
    sudo yum -y versionlock xrt-2.11.722
    sudo yum -y install xilinx-alveo-u30-core

    sudo cp /opt/xilinx/xcdr/xclbins/transcode.xclbin /opt/xilinx/xcdr/xclbins/transcode_lite.xclbin
    sudo cp /opt/xilinx/xcdr/xclbins/on_prem/transcode.xclbin /opt/xilinx/xcdr/xclbins/transcode.xclbin
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

echo "##########################################################################################"
echo " Reboot is required to use the xilinx video driver. "
echo " To activate the driver, enter the cli command below. "
echo " "
echo " Up to date"
echo " $ source /opt/xilinx/xrt/setup.sh"
echo " $ sudo /opt/xilinx/xrt/bin/xball --device-filter u30 xbmgmt program --base"
echo " "
echo " Setup the runtime environment"
echo " $ source /opt/xilinx/xcdr/setup.sh"
echo " "
echo " verify that the cards are correctly detected"
echo " $ xbutil examine"
echo "##########################################################################################"

