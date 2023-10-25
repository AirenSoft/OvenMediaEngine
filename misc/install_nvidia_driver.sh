#!/bin/bash

NVIDIA_DRIVER_VERSION=

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



echo "##########################################################################################"
echo " Install NVIDIA drivers and CUDA Toolkit"
echo "##########################################################################################"
echo ${OSTYPE} / ${OSNAME} / ${OSVERSION} . ${OSMINORVERSION}
echo ${CURRENT}


##########################################################################################
# Drivers for Ubuntu 18.04 / 20.04
##########################################################################################
install_base_ubuntu()
{
    sudo apt-get -y update
    sudo apt-get -y install --no-install-recommends apt-utils lshw
    sudo apt-get -y install --no-install-recommends keyboard-configuration
    sudo apt-get -y install --no-install-recommends ubuntu-drivers-common
    sudo apt-get -y install --no-install-recommends gnupg2 ca-certificates software-properties-common


    # Uninstalling a previously installed NVIDIA Driver
    sudo apt-get -y remove --purge nvidia-*
    sudo apt-get -y autoremove
    sudo apt-get -y update

    # Remove the nouveau driver.
    # If the nouveau driver is in use, the nvidia driver cannot be installed.
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

    # Install nvidia drivers and cuda-toolit
    sudo add-apt-repository ppa:graphics-drivers/ppa

    sudo apt -y update
    if [ -z "$NVIDIA_DRIVER_VERSION" ]
    then 
        # installation with recommended version
        sudo ubuntu-drivers autoinstall
    else
        # installation with specific version
        sudo apt-get install -y --no-install-recommends nvidia-driver-${NVIDIA_DRIVER_VERSION}
    fi     
    sudo apt-get install -y --no-install-recommends nvidia-cuda-toolkit
}

##########################################################################################
# Drivers for CentOS 7
##########################################################################################
install_base_centos7()
{
    yum -y update
    yum -y install kernel-devel
    yum -y install epel-release
    yum -y install dkms curl lshw
    yum -y install subscription-manager

    echo "Reboot is required to run with a new version of the kernel."

    # Remove the nouveau driver.
    USE_NOUVEAU=`lshw -class video | grep nouveau`
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

    subscription-manager repos --enable=rhel-7-server-optional-rpms
    yum-config-manager --add-repo https://developer.download.nvidia.com/compute/cuda/repos/rhel7/x86_64/cuda-rhel7.repo
    yum clean expire-cache

    yum -y install nvidia-driver-latest-dkms
    yum -y install cuda
    yum -y install cuda-drivers

    # Configure Envionment Variables
    echo "Please add the PATH below to the environment variable."
    echo ""
    echo "export PATH=${PATH}:/usr/local/cuda/bin/"
    echo ""
    export PATH=${PATH}:/usr/local/cuda/bin/
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

# Arguments parser
while (($#)); do
    OPT=$1
    shift
    case $OPT in
        --*) case ${OPT:2} in
            nvidia_driver) NVIDIA_DRIVER_VERSION=$1; shift ;;
        esac;;

        -*) case ${OPT:1} in
            n) NVIDIA_DRIVER_VERSION=$1; shift;;
        esac;;
    esac
done

if [ "${OSNAME}" == "Ubuntu" ]; then
    check_version
    install_base_ubuntu
elif  [ "${OSNAME}" == "CentOS" ]; then
    check_version

    if [[ "${OSVERSION}" == "7" ]]; then
            install_base_centos7
    elif [[ "${OSVERSION}" == "8" ]]; then
            echo "Deprecated"
    fi
else
    echo "This program [$0] does not support your operating system [${OSNAME}]"
    echo "Please refer to manual installation page"
fi

  
 

echo "##########################################################################################"
echo " Reboot is required to use the nvidia video driver"
echo "##########################################################################################"
