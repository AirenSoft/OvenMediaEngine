#!/bin/bash

PREFIX=/opt/ovenmediaengine
TEMP_PATH=/tmp

OME_VERSION=temp/alpine
OPENSSL_VERSION=1.1.0g
SRTP_VERSION=2.2.0
SRT_VERSION=1.3.3
OPUS_VERSION=1.1.3
X264_VERSION=20190513-2245-stable
VPX_VERSION=1.7.0
FDKAAC_VERSION=0.1.5
FFMPEG_VERSION=3.4.2

if [[ "$OSTYPE" == "darwin"* ]]; then
    NCPU=$(sysctl -n hw.ncpu)
    OSNAME=$(sw_vers -productName)
    OSVERSION=$(sw_vers -productVersion)
else
    NCPU=$(nproc)
    OSNAME=$(cat /etc/*-release | grep "^NAME" | tr -d "\"" | cut -d"=" -f2)
    OSVERSION=$(cat /etc/*-release | grep ^VERSION= | tr -d "\"" | cut -d"=" -f2 | cut -d"." -f1 | awk '{print  $1}')
fi
MAKEFLAGS="${MAKEFLAGS} -j${NCPU}"
CURRENT=$(pwd)

install_openssl()
{
    (DIR=${TEMP_PATH}/openssl && \
    mkdir -p ${DIR} && \
    cd ${DIR} && \
    curl -sLf https://www.openssl.org/source/openssl-${OPENSSL_VERSION}.tar.gz | tar -xz --strip-components=1 && \
    ./config --prefix="${PREFIX}" --openssldir="${PREFIX}" -Wl,-rpath,"${PREFIX}/lib" shared no-idea no-mdc2 no-rc5 no-ec2m no-ecdh no-ecdsa no-async && \
    make && \
    sudo make install_sw && \
    rm -rf ${DIR} && \
    sudo rm -rf ${PREFIX}/bin) || fail_exit "openssl"
}

install_libsrtp()
{
    (DIR=${TEMP_PATH}/srtp && \
    mkdir -p ${DIR} && \
    cd ${DIR} && \
    curl -sLf https://github.com/cisco/libsrtp/archive/v${SRTP_VERSION}.tar.gz | tar -xz --strip-components=1 && \
    ./configure --prefix="${PREFIX}" --enable-shared --disable-static --enable-openssl --with-openssl-dir="${PREFIX}" && \
    make shared_library&& \
    sudo make install && \
    rm -rf ${DIR}) || fail_exit "srtp"
}

install_libsrt()
{
    (DIR=${TEMP_PATH}/srt && \
    mkdir -p ${DIR} && \
    cd ${DIR} && \
    curl -sLf https://github.com/Haivision/srt/archive/v${SRT_VERSION}.tar.gz | tar -xz --strip-components=1 && \
    PKG_CONFIG_PATH=${PREFIX}/lib/pkgconfig:${PKG_CONFIG_PATH} ./configure --prefix="${PREFIX}" --enable-shared --disable-static && \
    make && \
    sudo make install && \
    rm -rf ${DIR} && \
    sudo rm -rf ${PREFIX}/bin) || fail_exit "srt"
}

install_libopus()
{
    (DIR=${TEMP_PATH}/opus && \
    mkdir -p ${DIR} && \
    cd ${DIR} && \
    curl -sLf https://archive.mozilla.org/pub/opus/opus-${OPUS_VERSION}.tar.gz | tar -xz --strip-components=1 && \
    autoreconf -fiv && \
    ./configure --prefix="${PREFIX}" --enable-shared --disable-static && \
    make && \
    sudo make install && \
    sudo rm -rf ${PREFIX}/share && \
    rm -rf ${DIR}) || fail_exit "opus"
}

install_libx264()
{
    (DIR=${TEMP_PATH}/x264 && \
    mkdir -p ${DIR} && \
    cd ${DIR} && \
    curl -sLf https://download.videolan.org/pub/videolan/x264/snapshots/x264-snapshot-${X264_VERSION}.tar.bz2 | tar -jx --strip-components=1 && \
    ./configure --prefix="${PREFIX}" --enable-shared --enable-pic --disable-cli && \
    make && \
    sudo make install && \
    rm -rf ${DIR}) || fail_exit "x264"
}

install_libvpx()
{
    ADDITIONAL_FLAGS=
    if [ "x${OSNAME}" == "xMac OS X" ]; then
        case $OSVERSION in
            10.12.* | 10.13.* | 10.14.*  | 10.15.*) ADDITIONAL_FLAGS=--target=x86_64-darwin16-gcc;;
        esac
    fi

    (DIR=${TEMP_PATH}/vpx && \
    mkdir -p ${DIR} && \
    cd ${DIR} && \
    curl -sLf https://codeload.github.com/webmproject/libvpx/tar.gz/v${VPX_VERSION} | tar -xz --strip-components=1 && \
    ./configure --prefix="${PREFIX}" --enable-vp8 --enable-pic --enable-shared --disable-static --disable-vp9 --disable-debug --disable-examples --disable-docs --disable-install-bins ${ADDITIONAL_FLAGS} && \
    make && \
    sudo make install && \
    rm -rf ${DIR}) || fail_exit "vpx"
}

install_fdk_aac()
{
    (DIR=${TEMP_PATH}/aac && \
    mkdir -p ${DIR} && \
    cd ${DIR} && \
    curl -sLf https://github.com/mstorsjo/fdk-aac/archive/v${FDKAAC_VERSION}.tar.gz | tar -xz --strip-components=1 && \
    autoreconf -fiv && \
    ./configure --prefix="${PREFIX}" --enable-shared --disable-static --datadir=/tmp/aac && \
    make && \
    sudo make install && \
    rm -rf ${DIR}) || fail_exit "fdk_aac"
}

install_ffmpeg()
{
    (DIR=${TEMP_PATH}/ffmpeg && \
    mkdir -p ${DIR} && \
    cd ${DIR} && \
    curl -sLf https://www.ffmpeg.org/releases/ffmpeg-${FFMPEG_VERSION}.tar.bz2 | tar -jx --strip-components=1 && \
    PKG_CONFIG_PATH=${PREFIX}/lib/pkgconfig:${PKG_CONFIG_PATH} ./configure \
    --prefix="${PREFIX}" \
    --enable-gpl \
    --enable-nonfree \
    --extra-cflags="-I${PREFIX}/include"  \
    --extra-ldflags="-L${PREFIX}/lib -Wl,-rpath,${PREFIX}/lib" \
    --extra-libs=-ldl \
    --enable-shared \
    --disable-static \
    --disable-debug \
    --disable-doc \
    --disable-programs \
    --disable-avdevice --disable-dct --disable-dwt --disable-error-resilience --disable-lsp --disable-lzo --disable-rdft --disable-faan --disable-pixelutils \
    --disable-everything \
    --enable-zlib --enable-libopus --enable-libvpx --enable-libfdk_aac --enable-libx264 \
    --enable-encoder=libvpx_vp8,libvpx_vp9,libopus,libfdk_aac,libx264 \
    --enable-decoder=aac,aac_latm,aac_fixed,h264 \
    --enable-parser=aac,aac_latm,aac_fixed,h264 \
    --enable-network --enable-protocol=tcp --enable-protocol=udp --enable-protocol=rtp --enable-demuxer=rtsp \
    --enable-filter=asetnsamples,aresample,aformat,channelmap,channelsplit,scale,transpose,fps,settb,asettb && \
    make && \
    sudo make install && \
    sudo rm -rf ${PREFIX}/share && \
    rm -rf ${DIR}) || fail_exit "ffmpeg"
}

install_base_ubuntu()
{
    sudo apt install -y build-essential nasm autoconf libtool zlib1g-dev tclsh cmake curl pkg-config
}

install_base_fedora()
{
    sudo yum install -y gcc-c++ make nasm autoconf libtool zlib-devel tcl cmake
}

install_base_centos()
{
    # for downloading latest version of nasm (x264 needs nasm 2.13+ but centos provides 2.10 )
    sudo curl -so /etc/yum.repos.d/nasm.repo https://www.nasm.us/nasm.repo
    # centos-release-scl should be installed before installing devtoolset-7
    sudo yum install -y centos-release-scl
    sudo yum install -y bc gcc-c++ cmake nasm autoconf libtool glibc-static tcl bzip2 zlib-devel devtoolset-7
    source scl_source enable devtoolset-7
}

install_base_macos()
{
    BREW_PATH=$(which brew)
    if [ ! -x "$BREW_PATH" ] ; then
        /usr/bin/ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)" || exit 1
    fi

    # the default make on macOS does not work with these makefiles
    brew install pkg-config nasm automake libtool xz cmake make

    # the nasm that comes with macOS does not work with libvpx thus put the path where the homebrew stuff is installed in front of PATH
    export PATH=/usr/local/bin:$PATH
}

install_ovenmediaengine()
{
    (DIR=${TEMP_PATH}/ome && \
    mkdir -p ${DIR} && \
    cd ${DIR} && \
    curl -sLf https://github.com/AirenSoft/OvenMediaEngine/archive/${OME_VERSION}.tar.gz | tar -xz --strip-components=1 && \
    cd src && \
    make release && \
    sudo make install && \
    rm -rf ${DIR}) || fail_exit "ovenmediaengine"
}

fail_exit()
{
    echo "($1) installation has failed."
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

if [ "x${OSNAME}" == "xUbuntu" ]; then
    check_version
    install_base_ubuntu
elif  [ "x${OSNAME}" == "xCentOS Linux" ]; then
    check_version
    install_base_centos
elif  [ "x${OSNAME}" == "xFedora" ]; then
    check_version
    install_base_fedora
elif  [ "x${OSNAME}" == "xMac OS X" ]; then
    install_base_macos
else
    echo "This program [$0] does not support your operating system [${OSNAME}]"
    echo "Please refer to manual installation page"
fi

install_openssl
install_libsrtp
install_libsrt
install_libopus
install_libx264
install_libvpx
install_fdk_aac
install_ffmpeg

if [ "with_ome" == "$1" ]; then
    install_ovenmediaengine
fi