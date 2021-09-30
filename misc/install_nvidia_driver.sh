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
    export DESTDIR=${PREFIX} && \
    curl -sLf https://github.com/FFmpeg/nv-codec-headers/releases/download/n${NVCC_HEADERS}/nv-codec-headers-${NVCC_HEADERS}.tar.gz | tar -xz --strip-components=1 && \
    sudo make install && \
    rm -rf ${DIR}) || fail_exit "nvcc_headers"
}

install_base_ubuntu()
{
    if [[ "${OSVERSION}" == "18" || "${OSVERSION}" == "20" ]]; then

        # Uninstalling a previously installed NVIDIA Driver
        sudo apt-get remove --purge nvidia-*
        sudo apt-get -y autoremove
        sudo apt-get -y update

        # Remove the nouveau driver. If the nouveau driver is in use, the nvidia driver cannot be installed.
        USE_NOUVEAU=`sudo lshw -class video | grep nouveau`
        if [ ! -z "$USE_NOUVEAU" ]; then

                # Disable nouveau Driver
                echo "blacklist nouveau" >> /etc/modprobe.d/blacklist.conf
                echo "blacklist lbm-nouveau" >> /etc/modprobe.d/blacklist.conf
                echo "options nouveau modeset=0" >> /etc/modprobe.d/blacklist.conf
                echo "alias nouveau off" >> /etc/modprobe.d/blacklist.conf
                echo "alias lbm-nouveau off" >> /etc/modprobe.d/blacklist.conf
                sudo update-initramfs -u
                echo "Using a driver display nouveau.Remove the driver and reboot.Reboot and installation script to rerun the nvidia display the driver to complete the installation."

                sleep 5s
                reboot
        fi

        # Install Nvidia Driver and Nvidia Toolkit
        sudo add-apt-repository ppa:graphics-drivers/ppa
        sudo apt -y update
        sudo apt-get install -y $(ubuntu-drivers devices | grep recommended | awk '{print $3}')
        sudo apt-get install -y nvidia-cuda-toolkit curl make
        
    fi
}


install_base_centos()
{
    if [[ "${OSVERSION}" == "7" ]]; then

        # Update Kernel
        yum -y update
        yum -y groupinstall "Development Tools"
        yum -y install kernel-devel
        yum -y install epel-release
        yum -y install dkms curl
        echo "Reboot is required to run with a new version of the kernel."

        # Remove the nouveau driver. If the nouveau driver is in use, the nvidia driver cannot be installed.
        USE_NOUVEAU=`sudo lshw -class video | grep nouveau`
        if [ ! -z "$USE_NOUVEAU" ]; then

                # Disable nouveau Driver
                sed "s/GRUB_CMDLINE_LINUX=\"\(.*\)\"/GRUB_CMDLINE_LINUX=\"\1 rd.driver.blacklist=nouveau nouveau.modeset=0\"/" /etc/default/grub
                grub2-mkconfig -o /boot/grub2/grub.cfg
                echo "blacklist nouveau" >> /etc/modprobe.d/blacklist.conf
                mv /boot/initramfs-$(uname -r).img /boot/initramfs-$(uname -r)-nouveau.img
                dracut /boot/initramfs-$(uname -r).img $(uname -r)

                echo "Using a driver display nouveau. so, remove the driver and reboot. "
                echo "After reboot and installation script to rerun the nvidia display the driver to complete the installation."

                sleep 5s
                reboot
        fi

        # Install Nvidia Driver
        # https://www.nvidia.com/en-us/drivers/unix/
        systemctl isolate multi-user.target
        wget -N https://us.download.nvidia.com/XFree86/Linux-x86_64/460.84/NVIDIA-Linux-x86_64-460.84.run
        sh ./NVIDIA-Linux-x86_64-460.84.run --ui=none --no-questions

        # Install Nvidia Toolkit
        # https://developer.nvidia.com/cuda-downloads
        wget -N https://developer.download.nvidia.com/compute/cuda/11.3.1/local_installers/cuda_11.3.1_465.19.01_linux.run
        sh cuda_11.3.1_465.19.01_linux.run --silent

        # Configure Envionment Variables
        echo "Please add the PATH below to the environment variable."
        echo ""
        echo "export PATH=${PATH}:/usr/local/cuda/bin/"
        echo ""
        export PATH=${PATH}:/usr/local/cuda/bin/

    elif [[ "${OSVERSION}" == "8" ]]; then

        dnf update -y
        dnf groupinstall -y "Development Tools" 
        dnf install -y elfutils-libelf-devel "kernel-devel-uname-r == $(uname -r)"

        # Remove the nouveau driver. If the nouveau driver is in use, the nvidia driver cannot be installed.
        USE_NOUVEAU=`sudo lshw -class video | grep nouveau`
        if [ ! -z "$USE_NOUVEAU" ]; then

                # Disable nouveau Driver
                echo "blacklist nouveau" >> /etc/modprobe.d/blacklist.conf
                mv /boot/initramfs-$(uname -r).img /boot/initramfs-$(uname -r)-nouveau.img
                dracut /boot/initramfs-$(uname -r).img $(uname -r)

                systemctl set-default multi-user.target
                systemctl get-default

                echo "Using a driver display nouveau. so, remove the driver and reboot. "
                echo "After reboot and installation script to rerun the nvidia display the driver to complete the installation."

                sleep 5s
                reboot
        fi

        wget -N https://us.download.nvidia.com/XFree86/Linux-x86_64/460.84/NVIDIA-Linux-x86_64-460.84.run
        sh ./NVIDIA-Linux-x86_64-460.84.run --ui=none --no-questions

        # Install Nvidia Toolkit
        # https://developer.nvidia.com/cuda-downloads
        wget -N https://developer.download.nvidia.com/compute/cuda/11.3.1/local_installers/cuda_11.3.1_465.19.01_linux.run
        sh cuda_11.3.1_465.19.01_linux.run --silent

        systemctl set-default graphical.target
        systemctl get-default

        # Configure Envionment Variables
        echo "Please add the PATH below to the environment variable."
        echo ""
        echo "export PATH=${PATH}:/usr/local/cuda/bin/"
        echo ""
        export PATH=${PATH}:/usr/local/cuda/bin/

    else
        fail_exit
    fi
}

install_base_amazonlinux()
{
    echo "TODO"
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
    read -p "This program [$0] is tested on [Ubuntu 18/20.04, CentOS 7/8] Do you want to continue [y/N] ? " ANS
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
    install_base_amazonlinux
else
    echo "This program [$0] does not support your operating system [${OSNAME}]"
    echo "Please refer to manual installation page"
fi

install_nvcc_headers

echo "-----------------------------------------------------"
echo " Reboot is required to use the nvidia video driver"
echo "-----------------------------------------------------"