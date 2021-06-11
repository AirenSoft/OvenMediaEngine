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
NVCC_HEADERS=11.0.10.1


install_nvcc_headers() {
    (DIR=${TEMP_PATH}/nvcc-hdr && \
    mkdir -p ${DIR} && \
    cd ${DIR} && \
    curl -sLf https://github.com/FFmpeg/nv-codec-headers/releases/download/n${NVCC_HEADERS}/nv-codec-headers-${NVCC_HEADERS}.tar.gz | tar -xz --strip-components=1 && \
    sudo make install && \
    rm -rf ${DIR}) || fail_exit "nvcc_headers"
}


install_base_ubuntu()
{
    # Install Nvidia Driver and Nvidia Toolkit
    add-apt-repository ppa:graphics-drivers/ppa
    apt update
    apt install -y nvidia-driver-460 nvidia-cuda-toolkit

    echo "Reboot is required to use the new video driver."
}

install_base_centos()
{
    if [[ "${OSVERSION}" == "7" ]]; then

        # Update Kernel
        yum -y update
        yum -y groupinstall "Development Tools"
        yum -y install kernel-devel
        yum -y install epel-release
        yum -y install dkms
        echo "Reboot is required to run with a new version of the kernel."

        # Disable nouveau Driver
        sed "s/GRUB_CMDLINE_LINUX=\"\(.*\)\"/GRUB_CMDLINE_LINUX=\"\1 rd.driver.blacklist=nouveau nouveau.modeset=0\"/" /etc/default/grub
        grub2-mkconfig -o /boot/grub2/grub.cfg
        echo "blacklist nouveau" >> /etc/modprobe.d/blacklist.conf
        mv /boot/initramfs-$(uname -r).img /boot/initramfs-$(uname -r)-nouveau.img
        dracut /boot/initramfs-$(uname -r).img $(uname -r)
        echo "Reboot is required to release the nouveau driver."
        
        # Install Nvidia Driver 
        # https://www.nvidia.com/en-us/drivers/unix/
        systemctl isolate multi-user.target
        wget https://us.download.nvidia.com/XFree86/Linux-x86_64/460.84/NVIDIA-Linux-x86_64-460.84.run
        sh ./NVIDIA-Linux-x86_64-460.84.run --ui=none --no-questions

        # Install Nvidia Toolkit
        # https://developer.nvidia.com/cuda-downloads
        wget https://developer.download.nvidia.com/compute/cuda/11.3.1/local_installers/cuda_11.3.1_465.19.01_linux.run
        sh cuda_11.3.1_465.19.01_linux.run --silent

        # Configure Envionment Variables
        export PATH=${PATH}:/usr/local/cuda/bin/

    elif [[ "${OSVERSION}" == "8" ]]; then
        
        echo "TODO"

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

