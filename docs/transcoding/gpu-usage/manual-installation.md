# Manual Installation

## Intel QuickSync Driver Installation

### Platform Specific Installation

{% tabs %}
{% tab title="Ubuntu 18/20" %}
```
sudo apt install -y libdrm-dev xorg xorg-dev openbox libx11-dev 
sudo apt install -y libgl1-mesa-glx libgl1-mesa-dev
```
{% endtab %}

{% tab title="CentOS 7" %}
```
# Centos 7 uses the 2.8.x version of cmake by default. 
# It must be changed to version 3.x or higher.
sudo yum remove -y cmake
sudo yum install -y cmake3
sudo ln -s /usr/bin/cmake3 /usr/bin/cmake

sudo yum install -y libdrm-devel libX11-devel libXi-devel
```
{% endtab %}

{% tab title="CentOS 8" %}
```
sudo yum install -y libdrm-devel libX11-devel libXi-devel
```
{% endtab %}
{% endtabs %}

### Common Installation

#### LibVA

```bash
PREFIX=/opt/ovenmediaengine && \
TEMP_PATH=/tmp && \
LIBVA_VERSION=2.11.0 && \
DIR=${TEMP_PATH}/libva && \
mkdir -p ${DIR} && \
cd ${DIR} && \
curl -sLf https://github.com/intel/libva/archive/refs/tags/${LIBVA_VERSION}.tar.gz | tar -xz --strip-components=1 && \
./autogen.sh --prefix="${PREFIX}" && \
make -j$(nproc) && \
sudo make install && \
rm -rf ${DIR}) 
```

#### GMMLIB

```bash
PREFIX=/opt/ovenmediaengine && \
TEMP_PATH=/tmp && \
GMMLIB_VERSION=20.4.1 && \
DIR=${TEMP_PATH}/gmmlib && \
mkdir -p ${DIR} && \
cd ${DIR} && \
curl -sLf https://github.com/intel/gmmlib/archive/refs/tags/intel-gmmlib-${GMMLIB_VERSION}.tar.gz | tar -xz --strip-components=1 && \
mkdir -p ${DIR}/build && \
cd ${DIR}/build && \
cmake -DCMAKE_INSTALL_PREFIX="${PREFIX}" .. && \
make -j$(nproc) && \
sudo make install && \
rm -rf ${DIR}
```

#### &#x20;Intel Media Driver&#x20;

```bash
PREFIX=/opt/ovenmediaengine && \
TEMP_PATH=/tmp && \
INTEL_MEDIA_DRIVER_VERSION=20.4.5 && \
DIR_IMD=${TEMP_PATH}/media-driver && \
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
rm -rf ${DIR_GMMLIB}

export LIBVA_DRIVERS_PATH=${PREFIX}/lib
export LIBVA_DRIVER_NAME=iHD
```

#### Intel Media SDK

```bash
PREFIX=/opt/ovenmediaengine && \
TEMP_PATH=/tmp && \
INTEL_MEDIA_SDK_VERSION=20.5.1 && \
DIR=${TEMP_PATH}/medka-sdk && \
mkdir -p ${DIR} && \
cd ${DIR} && \
curl -sLf https://github.com/Intel-Media-SDK/MediaSDK/archive/refs/tags/intel-mediasdk-${INTEL_MEDIA_SDK_VERSION}.tar.gz  | tar -xz --strip-components=1 && \
mkdir -p ${DIR}/build && \
cd ${DIR}/build && \
PKG_CONFIG_PATH=${PREFIX}/lib/pkgconfig:${PKG_CONFIG_PATH} cmake -DCMAKE_INSTALL_PREFIX="${PREFIX}" .. && \
make -j$(nproc) && \
sudo make install && \
rm -rf ${DIR}
```

{% hint style="info" %}
All libraries are installed, the system must be rebooted.
{% endhint %}

## NVIDIA Driver Installation

### Platform Specific Installation

{% tabs %}
{% tab title="Ubuntu 18/20" %}
```
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
```
{% endtab %}

{% tab title="CentOS 7" %}
```
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
```
{% endtab %}

{% tab title="CentOS 8" %}
```
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
```
{% endtab %}
{% endtabs %}

### Common Installation

#### NVCC Headers

```bash
PREFIX=/opt/ovenmediaengine && \
TEMP_PATH=/tmp && \
NVCC_HEADERS=11.0.10.1 && \
DIR=${TEMP_PATH}/nvcc-hdr && \
mkdir -p ${DIR} && \
cd ${DIR} && \
curl -sLf https://github.com/FFmpeg/nv-codec-headers/releases/download/n${NVCC_HEADERS}/nv-codec-headers-${NVCC_HEADERS}.tar.gz | tar -xz --strip-components=1 && \
sudo make install && \
rm -rf ${DIR}
```

## NVIDIA Container Toolkit Installation

### Platform Specific Installation

{% tabs %}
{% tab title="Ubuntu 18/20" %}
```
# Install Docker 2.x
curl https://get.docker.com && sudo systemctl --now enable docker

# Install NVIDIA Docker Toolkit
distribution=$(. /etc/os-release;echo $ID$VERSION_ID) && \
curl -s -L https://nvidia.github.io/nvidia-docker/gpgkey | sudo apt-key add - && \
curl -s -L https://nvidia.github.io/nvidia-docker/$distribution/nvidia-docker.list | sudo tee /etc/apt/sources.list.d/nvidia-docker.list

sudo apt-get update
sudo apt-get install -y nvidia-docker2

# Restart Docker
sudo systemctl restart docker

# Test
sudo docker run --rm --gpus all nvidia/cuda:11.0-base nvidia-smi
```
{% endtab %}

{% tab title="CentOS 7" %}
```
sudo yum install -y yum-utils
sudo yum-config-manager --add-repo=https://download.docker.com/linux/centos/docker-ce.repo
sudo yum repolist -v
sudo yum install -y https://download.docker.com/linux/centos/7/x86_64/stable/Packages/containerd.io-1.4.3-3.1.el7.x86_64.rpm
sudo yum install docker-ce -y
sudo systemctl --now enable docker

# Install NVIDIA Docker Toolkit
distribution=$(. /etc/os-release;echo $ID$VERSION_ID) && \
curl -s -L https://nvidia.github.io/nvidia-docker/$distribution/nvidia-docker.repo | sudo tee /etc/yum.repos.d/nvidia-docker.repo

if [[ "${OSVERSION}" == "7" ]]; then
    sudo yum clean expire-cache
    sudo yum install -y nvidia-docker2
elif [[ "${OSVERSION}" == "8" ]]; then
    sudo dnf clean expire-cache --refresh
    sudo dnf install -y nvidia-docker2
else
    fail_exit
fi    

# Restart Docker
sudo systemctl restart docker    

# Test
sudo docker run --rm --gpus all nvidia/cuda:11.0-base nvidia-smi

```
{% endtab %}

{% tab title="CentOS 8" %}
```
sudo dnf install -y dnf-utils
sudo dnf config-manager --add-repo=https://download.docker.com/linux/centos/docker-ce.repo
sudo dnf repolist -v
sudo dnf install -y https://download.docker.com/linux/centos/7/x86_64/stable/Packages/containerd.io-1.4.3-3.1.el7.x86_64.rpm
sudo dnf install docker-ce -y
sudo systemctl --now enable docker

# Install NVIDIA Docker Toolkit
distribution=$(. /etc/os-release;echo $ID$VERSION_ID) && \
curl -s -L https://nvidia.github.io/nvidia-docker/$distribution/nvidia-docker.repo | sudo tee /etc/yum.repos.d/nvidia-docker.repo

if [[ "${OSVERSION}" == "7" ]]; then
    sudo yum clean expire-cache
    sudo yum install -y nvidia-docker2
elif [[ "${OSVERSION}" == "8" ]]; then
    sudo dnf clean expire-cache --refresh
    sudo dnf install -y nvidia-docker2
else
    fail_exit
fi    

# Restart Docker
sudo systemctl restart docker    

# Test
sudo docker run --rm --gpus all nvidia/cuda:11.0-base nvidia-smi
```
{% endtab %}
{% endtabs %}

