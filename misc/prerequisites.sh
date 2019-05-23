#!/bin/bash

OSNAME=$(cat /etc/*-release | grep "^NAME" | tr -d "\"" | cut -d"=" -f2)
OSVERSION=$(cat /etc/*-release | grep ^VERSION= | tr -d "\"" | cut -d"=" -f2 | cut -d"." -f1 | awk '{print  $1}')
WORKDIR=/tmp/ovenmediaengine
NCPU=$(nproc)
MAKE="make -j${NCPU}"
CURRENT=$(pwd)

install_libsrt()
{
    BUILD_PRG="LIBSRT"

    cd ${WORKDIR}

    if [ ! -d ${BUILD_PRG} ]; then
        curl -OLf https://github.com/Haivision/srt/archive/v1.3.1.tar.gz || fail_exit ${BUILD_PRG}
        tar xvf v1.3.1.tar.gz -C ${WORKDIR} && rm -rf v1.3.1.tar.gz && mv srt-* ${BUILD_PRG}
    fi

    cd ${BUILD_PRG}

    ./configure

    ${MAKE} && sudo make install
}

install_libsrtp()
{
    BUILD_PRG="LIBSRTP"

    cd ${WORKDIR}

    if [ ! -d ${BUILD_PRG} ]; then
        curl -OLkf https://github.com/cisco/libsrtp/archive/v2.2.0.tar.gz || fail_exit ${BUILD_PRG}
        tar xvfz v2.2.0.tar.gz -C ${WORKDIR} && rm -rf v2.2.0.tar.gz && mv libsrtp-* ${BUILD_PRG}
    fi

    cd ${BUILD_PRG}

    ./configure --enable-openssl

    ${MAKE} && sudo make install
}

install_fdk_aac()
{
    BUILD_PRG="FDKAAC"

    cd ${WORKDIR}

    if [ ! -d ${BUILD_PRG} ]; then
        curl -OLf https://github.com/mstorsjo/fdk-aac/archive/v0.1.5.tar.gz  || fail_exit ${BUILD_PRG}
        tar xvf v0.1.5.tar.gz -C ${WORKDIR} && rm -rf v0.1.5.tar.gz && mv fdk-aac-* ${BUILD_PRG}
    fi

    cd ${BUILD_PRG}

    ./autogen.sh && ./configure

    ${MAKE} && sudo make install
}

install_libopus()
{
    BUILD_PRG="OPUS"

    cd ${WORKDIR}

    if [ ! -d ${BUILD_PRG} ]; then
        curl -OLf https://archive.mozilla.org/pub/opus/opus-1.1.3.tar.gz  || fail_exit ${BUILD_PRG}
        tar xvfz opus-1.1.3.tar.gz -C ${WORKDIR} && rm -rf opus-1.1.3.tar.gz && mv opus-* ${BUILD_PRG}
    fi

    cd ${BUILD_PRG}

    autoreconf -f -i && ./configure --enable-shared --disable-static

    ${MAKE} && sudo make install
}

install_libvpx()
{
    BUILD_PRG="LIBVPX"

    cd ${WORKDIR}

    if [ ! -d ${BUILD_PRG} ]; then
        curl -OLf https://chromium.googlesource.com/webm/libvpx/+archive/v1.7.0.tar.gz  || fail_exit ${BUILD_PRG}
        mkdir ${BUILD_PRG}
        tar xvfz v1.7.0.tar.gz -C ${WORKDIR}/${BUILD_PRG} && rm -rf v1.7.0.tar.gz
    fi

    cd ${BUILD_PRG}

    ./configure --enable-shared --disable-static --disable-examples

    ${MAKE} && sudo make install
}

install_openssl()
{
    BUILD_PRG="OPENSSL"

    cd ${WORKDIR}

    if [ ! -d ${BUILD_PRG} ]; then
        curl -OLf https://www.openssl.org/source/openssl-1.1.0g.tar.gz  || fail_exit ${BUILD_PRG}
        tar xvfz openssl-1.1.0g.tar.gz -C ${WORKDIR} && rm -rf openssl-1.1.0g.tar.gz && mv openssl-* ${BUILD_PRG}
    fi

    cd ${BUILD_PRG}

    ./config shared no-idea no-mdc2 no-rc5 no-ec2m no-ecdh no-ecdsa

    ${MAKE} && sudo make install
}

install_ffmpeg()
{
    BUILD_PRG="FFMPEG"

    cd ${WORKDIR}

    if [ ! -d ${BUILD_PRG} ]; then
        curl -OLkf https://www.ffmpeg.org/releases/ffmpeg-3.4.2.tar.xz  || fail_exit ${BUILD_PRG}
        xz -d ffmpeg-3.4.2.tar.xz && tar xvf ffmpeg-3.4.2.tar -C ${WORKDIR} && rm -rf xvf ffmpeg-3.4.2.tar
        mv ffmpeg-3.4.2 ${BUILD_PRG}
    fi

    cd ${BUILD_PRG}

    ./configure \
        --disable-static --enable-shared \
        --extra-libs=-ldl \
        --enable-ffprobe \
        --disable-ffplay --disable-ffserver --disable-filters --disable-vaapi --disable-avdevice --disable-doc --disable-symver \
        --disable-debug --disable-indevs --disable-outdevs --disable-devices --disable-hwaccels --disable-encoders \
        --enable-zlib --enable-libopus --enable-libvpx --enable-libfdk_aac \
        --enable-encoder=libvpx_vp8,libvpx_vp9,libopus,libfdk_aac \
        --disable-decoder=tiff \
        --enable-filter=asetnsamples,aresample,aformat,channelmap,channelsplit,scale,transpose,fps,settb,asettb

    ${MAKE} && sudo make install
}

install_libopenh264()
{
    BUILD_PRG="OPENH264"

    cd ${WORKDIR}

    curl -OLf http://ciscobinary.openh264.org/libopenh264-1.8.0-linux64.4.so.bz2  || fail_exit ${BUILD_PRG}

    bzip2 -d libopenh264-1.8.0-linux64.4.so.bz2

    sudo mv libopenh264-1.8.0-linux64.4.so /usr/lib

    sudo ln -sf /usr/lib/libopenh264-1.8.0-linux64.4.so /usr/lib/libopenh264.so

    sudo ln -sf /usr/lib/libopenh264-1.8.0-linux64.4.so /usr/lib/libopenh264.so.4
}

install_base_ubuntu()
{
    PKGS="build-essential nasm autoconf libtool zlib1g-dev libssl-dev libvpx-dev libopus-dev pkg-config libfdk-aac-dev \
tclsh cmake curl"
    for PKG in ${PKGS}; do
        sudo apt install -y ${PKG} || fail_exit ${PKG}
    done
}

install_base_fedora()
{
    PKGS="gcc-c++ make nasm autoconf libtool zlib-devel openssl-devel libvpx-devel opus-devel tcl cmake"
    for PKG in ${PKGS}; do
        sudo yum install -y ${PKG} || fail_exit ${PKG}
    done

    export PKG_CONFIG_PATH=\${PKG_CONFIG_PATH}:/usr/local/lib/pkgconfig:/usr/local/lib64/pkgconfig
    export LD_LIBRARY_PATH=\${LD_LIBRARY_PATH}:/usr/local/lib:/usr/local/lib64
}

install_base_centos()
{
    PKGS="centos-release-scl bc gcc-c++ nasm autoconf libtool glibc-static zlib-devel git bzip2 tcl cmake devtoolset-7"
    for PKG in ${PKGS}; do
        sudo yum install -y ${PKG} || fail_exit ${PKG}
    done

    source scl_source enable devtoolset-7

    export PKG_CONFIG_PATH=\${PKG_CONFIG_PATH}:/usr/local/lib/pkgconfig:/usr/local/lib64/pkgconfig
    export LD_LIBRARY_PATH=\${LD_LIBRARY_PATH}:/usr/local/lib:/usr/local/lib64
}

fail_exit()
{
    echo "Software ($1) download failed."
    cd ${CURRENT}
    exit 1
}

check_version()
{
    if [[ "x${OSNAME}" == "xUbuntu" && "x${OSVERSION}" != "x18" ]]; then
        proceed_yn
    fi

    if [[ "x${OSNAME}" == "xCentOS Linux" && "x${OSVERSION}" != "x7" ]]; then
        proceed_yn
    fi

    if [[ "x${OSNAME}" == "xFedora" && "x${OSVERSION}" != "x28" ]]; then
        proceed_yn
    fi
}

proceed_yn()
{
    read -p "This program [$0] is tested on [Ubuntu 18, CentOS 7, Fedora 28]
Do you want to continue [y/N] ? " ANS
    if [[ "x${ANS}" != "xy" && "x$ANS" != "xyes" ]]; then
        cd ${CURRENT}
        exit 1
    fi
}

mkdir -p ${WORKDIR}

if [ "x${OSNAME}" == "xUbuntu" ]; then

    check_version

    install_base_ubuntu

    install_libsrtp

    install_libopenh264

    install_libsrt

    install_ffmpeg

    sudo ldconfig
elif  [ "x${OSNAME}" == "xCentOS Linux" ]; then

    check_version

    install_base_centos

    install_openssl

    install_libvpx

    install_libopus

    install_libsrtp

    install_fdk_aac

    install_ffmpeg

    install_libopenh264

    install_libsrt

    sudo ldconfig
elif  [ "x${OSNAME}" == "xFedora" ]; then

    check_version

    install_base_fedora

    install_libsrtp

    install_fdk_aac

    install_ffmpeg

    install_libopenh264

    install_libsrt

    sudo ldconfig
else
    echo "This program [$0] does not support your operating system [${OSNAME}]"
    echo "Please refer to manual installation page"
fi

cd ${CURRENT}