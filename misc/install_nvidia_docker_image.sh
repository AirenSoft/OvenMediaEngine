#!/bin/bash
CUDA_VERSION=11.3.0

## Install build utils
apt-get -y install build-essential nasm autoconf libtool zlib1g-dev tclsh cmake curl pkg-config bc uuid-dev apt-utils

apt-get install -y --no-install-recommends gnupg2 curl ca-certificates
curl -fsSL https://developer.download.nvidia.com/compute/cuda/repos/ubuntu1804/x86_64/7fa2af80.pub | apt-key add -
echo "deb https://developer.download.nvidia.com/compute/cuda/repos/ubuntu1804/x86_64 /" > /etc/apt/sources.list.d/cuda.list
echo "deb https://developer.download.nvidia.com/compute/machine-learning/repos/ubuntu1804/x86_64 /" > /etc/apt/sources.list.d/nvidia-ml.list
rm -rf /var/lib/apt/lists/*

## For libraries in the cuda-compat-* package:
## https://docs.nvidia.com/cuda/eula/index.html#attachment-a
apt-get update 
apt-get install -y --no-install-recommends cuda-cudart-11-3=11.3.58-1 cuda-compat-11-3 
ln -s cuda-11.3 /usr/local/cuda 
rm -rf /var/lib/apt/lists/*

## Required for nvidia-docker
echo "/usr/local/nvidia/lib" >> /etc/ld.so.conf.d/nvidia.conf
echo "/usr/local/nvidia/lib64" >> /etc/ld.so.conf.d/nvidia.conf
export PATH=/usr/local/nvidia/bin:/usr/local/cuda/bin:${PATH}
export LD_LIBRARY_PATH=/usr/local/nvidia/lib:/usr/local/nvidia/lib64:${LD_LIBRARY_PATH}

## NVIDIA Runtime Container
NVIDIA_VISIBLE_DEVICES=all
NVIDIA_DRIVER_CAPABILITIES=compute,utility
NVIDIA_REQUIRE_CUDA="cuda>=11.3 brand=tesla,driver>=418,driver<419 brand=tesla,driver>=440,driver<441 driver>=450"
NCCL_VERSION=2.9.6

apt-get update && \
apt-get install -y --no-install-recommends \
    cuda-libraries-11-3=11.3.0-1 \
    libnpp-11-3=11.3.3.44-1 \
    cuda-nvtx-11-3=11.3.58-1 \
    libcublas-11-3=11.4.2.10064-1 \
    libcusparse-11-3=11.5.0.58-1 \
    libnccl2=$NCCL_VERSION-1+cuda11.3 && \
rm -rf /var/lib/apt/lists/*

## apt from auto upgrading the cublas package.
## See https://gitlab.com/nvidia/container-images/cuda/-/issues/88
apt-mark hold libcublas-11-3 libnccl2 libcublas-dev-11-3 libnccl-dev
#LIBRARY_PATH=/usr/local/cuda/lib64/stubs

apt-get update && apt-get install -y --no-install-recommends \
    cuda-cudart-dev-11-3=11.3.58-1 \
    cuda-command-line-tools-11-3=11.3.0-1 \
    cuda-minimal-build-11-3=11.3.0-1 \
    cuda-libraries-dev-11-3=11.3.0-1 \
    cuda-nvml-dev-11-3=11.3.58-1 \
    libnpp-dev-11-3=11.3.3.44-1 \
    libnccl-dev=2.9.6-1+cuda11.3 \
    libcublas-dev-11-3=11.4.2.10064-1 \
    libcusparse-dev-11-3=11.5.0.58-1 \
    && rm -rf /var/lib/apt/lists/*

## Build NVCC Header
NVCC_HEADERS=11.0.10.1
DIR=/tmp/nvcc-hdr && \
    mkdir -p ${DIR} && \
    cd ${DIR} && \
    curl -sLf https://github.com/FFmpeg/nv-codec-headers/releases/download/n${NVCC_HEADERS}/nv-codec-headers-${NVCC_HEADERS}.tar.gz | tar -xz --strip-components=1 && \
    make install && \
    rm -rf ${DIR} 
