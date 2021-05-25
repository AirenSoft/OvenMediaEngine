#!/bin/bash

PREFIX=/opt/ovenmediaengine
TEMP_PATH=/tmp

OME_VERSION=dev
OPENSSL_VERSION=1.1.1i
SRTP_VERSION=2.2.0
SRT_VERSION=1.4.2
OPUS_VERSION=1.1.3
X264_VERSION=20190513-2245-stable
X265_VERSION=3.4
VPX_VERSION=1.7.0
FDKAAC_VERSION=0.1.5
NASM_VERSION=2.15.02
FFMPEG_VERSION=4.3.2
JEMALLOC_VERSION=5.2.1
PCRE2_VERSION=10.35
# Support to Intel QuickSync hardware accelerator
LIBVA_VERSION=2.11.0
GMMLIB_VERSION=20.4.1
INTEL_MEDIA_DRIVER_VERSION=20.4.5
INTEL_MEDIA_SDK_VERSION=20.5.1
# Support to NVIDIA hardware accelerator
NVCC_HEADERS=11.0.10.1

ENABLE_QSV_HWACCELS=true
ENABLE_NVCC_HWACCELS=false

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

MAKEFLAGS="${MAKEFLAGS} -j${NCPU}"
CURRENT=$(pwd)

install_openssl()
{
    DOWNLOAD_NAME=${OPENSSL_VERSION//\./_}
    DOWNLOAD_URL="https://codeload.github.com/openssl/openssl/tar.gz/OpenSSL_${DOWNLOAD_NAME}"

    (DIR=${TEMP_PATH}/openssl && \
    mkdir -p ${DIR} && \
    cd ${DIR} && \
    curl -sLf "${DOWNLOAD_URL}" | tar -xz --strip-components=1 && \
    ./config --prefix="${PREFIX}" --openssldir="${PREFIX}" -Wl,-rpath,"${PREFIX}/lib" shared no-idea no-mdc2 no-rc5 no-ec2m no-ecdh no-ecdsa no-async && \
    make -j$(nproc) && \
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
    make -j$(nproc) shared_library&& \
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
    make -j$(nproc) && \
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
    make -j$(nproc) && \
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
    make -j$(nproc) && \
    sudo make install && \
    rm -rf ${DIR}) || fail_exit "x264"
}

install_libx265()
{
    (DIR=${TEMP_PATH}/x265 && \
    mkdir -p ${DIR} && \
    cd ${DIR} && \
    curl -sLf https://github.com/videolan/x265/archive/${X265_VERSION}.tar.gz | tar -xz --strip-components=1 && \
    cd ${DIR}/build/linux && \
    cmake -G "Unix Makefiles" -DCMAKE_INSTALL_PREFIX="${PREFIX}" -DENABLE_SHARED:bool=on ../../source && \
    make -j$(nproc) && \
    sudo make install && \
    rm -rf ${DIR}) || fail_exit "x265"
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
    make -j$(nproc) && \
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
    make -j$(nproc) && \
    sudo make install && \
    rm -rf ${DIR}) || fail_exit "fdk_aac"
}

install_nasm()
{
	# NASM is binary, so don't install it in the prefix. If this conflicts with the NASM installed on your system, you must install it yourself to avoid crashing.
	(DIR=${TEMP_PATH}/nasm && \
    mkdir -p ${DIR} && \
    cd ${DIR} && \
    curl -sLf http://www.nasm.us/pub/nasm/releasebuilds/${NASM_VERSION}/nasm-${NASM_VERSION}.tar.gz | tar -xz --strip-components=1 && \
    ./configure && \
    make -j$(nproc) && \
    sudo make install && \
    rm -rf ${DIR}) || fail_exit "nasm"
}

install_ffmpeg()
{
    # If you need debug symbols, put the options below.
    #  --extra-cflags="-I${PREFIX}/include -g"  \
    #  --enable-debug \
    #  --disable-optimizations --disable-mmx --disable-stripping \

    ADDI_LIBS=""
    ADDI_ENCODER=""
    ADDI_DECODER=""
    ADDI_CFLAGS=""
    ADDI_LDFLAGS=""
    ADDI_HWACCEL=""

    if [ "$ENABLE_QSV_HWACCELS" = true ] ; then
        ADDI_LIBS+=" --enable-libmfx"
        ADDI_ENCODER+=",h264_qsv,hevc_qsv"
        ADDI_DECODER+=",vp8_qsv,h264_qsv,hevc_qsv"
    fi

    if [ "$ENABLE_NVCC_HWACCELS" = true ] ; then
        ADDI_LIBS+=" --enable-cuda-nvcc --enable-libnpp"
        ADDI_ENCODER+=",h264_nvenc,hevc_nvenc"
        ADDI_DECODER+=""
        ADDI_CFLAGS+="-I/usr/local/cuda/include"
        ADDI_LDFLAGS="-L/usr/local/cuda/lib64"
        ADDI_HWACCEL="h264_nvdec,hevc_nvdec"
    fi

    (DIR=${TEMP_PATH}/ffmpeg && \
    mkdir -p ${DIR} && \
    cd ${DIR} && \
    curl -sLf https://github.com/FFmpeg/FFmpeg/archive/refs/tags/n${FFMPEG_VERSION}.tar.gz | tar -xz --strip-components=1 && \
    PKG_CONFIG_PATH=${PREFIX}/lib/pkgconfig:${PREFIX}/lib64/pkgconfig:${PKG_CONFIG_PATH} ./configure \
    --prefix="${PREFIX}" \
    --enable-gpl \
    --enable-nonfree \
    --extra-cflags="-I${PREFIX}/include ${ADDI_CFLAGS}"  \
    --extra-ldflags="-L${PREFIX}/lib ${ADDI_LDFLAGS} -Wl,-rpath,${PREFIX}/lib" \
    --extra-libs=-ldl \
    --enable-shared \
    --disable-static \
    --disable-debug \
    --disable-doc \
    --disable-programs  \
    --disable-avdevice --disable-dct --disable-dwt --disable-lsp --disable-lzo --disable-rdft --disable-faan --disable-pixelutils \
    --enable-zlib --enable-libopus --enable-libvpx --enable-libfdk_aac --enable-libx264 --enable-libx265 ${ADDI_LIBS} \
    --disable-everything \
    --disable-fast-unaligned \
    --enable-hwaccel=${ADDI_HWACCEL}  \
    --enable-encoder=libvpx_vp8,libopus,libfdk_aac,libx264,libx265,mjpeg,png${ADDI_ENCODER} \
    --enable-decoder=aac,aac_latm,aac_fixed,h264,hevc,opus,vp8${ADDI_DECODER} \
    --enable-parser=aac,aac_latm,aac_fixed,h264,hevc,opus,vp8 \
    --enable-network --enable-protocol=tcp --enable-protocol=udp --enable-protocol=rtp,file,rtmp --enable-demuxer=rtsp --enable-muxer=mp4,webm,mpegts,flv,mpjpeg \
    --enable-filter=asetnsamples,aresample,aformat,channelmap,channelsplit,scale,transpose,fps,settb,asettb,format && \
    make -j$(nproc) && \
    sudo make install && \
    sudo rm -rf ${PREFIX}/share && \
    rm -rf ${DIR}) || fail_exit "ffmpeg"
}

install_jemalloc()
{
    (DIR=${TEMP_PATH}/jemalloc && \
    mkdir -p ${DIR} && \
    cd ${DIR} && \
    curl -sLf https://github.com/jemalloc/jemalloc/releases/download/${JEMALLOC_VERSION}/jemalloc-${JEMALLOC_VERSION}.tar.bz2 | tar -jx --strip-components=1 && \
    ./configure --prefix="${PREFIX}" && \
    make -j$(nproc) && \
    sudo make install_include install_lib && \
    rm -rf ${DIR}) || fail_exit "jemalloc"
}

install_libpcre2()
{
    (DIR=${TEMP_PATH}/libpcre2 && \
    mkdir -p ${DIR} && \
    cd ${DIR} && \
    curl -sLf https://ftp.pcre.org/pub/pcre/pcre2-${PCRE2_VERSION}.tar.gz | tar -xz --strip-components=1 && \
    ./configure --prefix="${PREFIX}" \
    --disable-static \
	--enable-jit=auto && \
    make -j$(nproc) && \
    sudo make install && \
    rm -rf ${DIR} && \
    sudo rm -rf ${PREFIX}/bin) || fail_exit "libpcre2"
}

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

install_nvidia_driver() {
    add-apt-repository ppa:graphics-drivers/ppa
    apt update
    apt install nvidia-driver-460 nvidia-cuda-toolkit
}

install_nvcc_headers() {
    (DIR=${TEMP_PATH}/gmmlib && \
    mkdir -p ${DIR} && \
    cd ${DIR} && \
    curl -sLf https://github.com/FFmpeg/nv-codec-headers/releases/download/n${NVCC_HEADERS}/nv-codec-headers-${NVCC_HEADERS}.tar.gz | tar -xz --strip-components=1 && \
    sudo make install && \
    rm -rf ${DIR}) || fail_exit "nvcc_headers"        
}

# Reboot is required after library installation
install_nvcc() {
    if [ "$ENABLE_NVCC_HWACCELS" = true ] ; then
        install_nvidia_driver
        install_nvcc_headers
    fi
}

install_qsv() {
    if [ "$ENABLE_QSV_HWACCELS" = true ] ; then
        install_libva
        install_gmmlib
        install_intel_media_driver
        install_intel_media_sdk
    fi
}

install_base_ubuntu()
{
    sudo apt install -y build-essential autoconf libtool zlib1g-dev tclsh cmake curl pkg-config bc
    
    # Dependency library for hardware accelerators
    sudo apt install -y libdrm-dev xorg xorg-dev openbox libx11-dev libgl1-mesa-glx libgl1-mesa-dev
}

install_base_fedora()
{
    sudo yum install -y gcc-c++ make autoconf libtool zlib-devel tcl cmake bc
}

install_base_centos()
{
    if [[ "${OSVERSION}" == "7" ]]; then
        # centos-release-scl should be installed before installing devtoolset-7
        sudo yum install -y centos-release-scl
        sudo yum install -y glibc-static devtoolset-7

        # Centos 7 uses the 2.8.x version of cmake by default. It must be changed to version 3.x or higher.
        sudo yum remove -y cmake
        sudo yum install -y cmake3
        sudo ln -s /usr/bin/cmake3 /usr/bin/cmake
        source scl_source enable devtoolset-7
    else
        sudo yum install -y cmake    
    fi

    sudo yum install -y bc gcc-c++ autoconf libtool tcl bzip2 zlib-devel 

    # Dependency library for hardware accelerator
    sudo yum install -y libdrm-devel libX11-devel libXi-devel
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
    if [[ "${OSNAME}" == "Ubuntu" && "${OSVERSION}" != "18" && "${OSVERSION}.${OSMINORVERSION}" != "20.04" ]]; then
        proceed_yn
    fi

    if [[ "${OSNAME}" == "CentOS" && "${OSVERSION}" != "7" && "${OSVERSION}" != "8" ]]; then
        proceed_yn
    fi

    if [[ "${OSNAME}" == "Fedora" && "${OSVERSION}" != "28" ]]; then
        proceed_yn
    fi
}

proceed_yn()
{
    read -p "This program [$0] is tested on [Ubuntu 18/20.04, CentOS 7/8 q, Fedora 28]
Do you want to continue [y/N] ? " ANS
    if [[ "${ANS}" != "y" && "$ANS" != "yes" ]]; then
        cd ${CURRENT}
        exit 1
    fi
}

for i in "$@"
do
case $i in
    -o=*|--os=*)
    OSNAME="${i#*=}"
    OSVERSION=""
    shift
    ;;
    -a|--all)
    WITH_OME="true"
    shift
    ;;
    *)
            # unknown option
    ;;
esac
done

if [ "${OSNAME}" == "Ubuntu" ]; then
    check_version
    # install_base_ubuntu
elif  [ "${OSNAME}" == "CentOS" ]; then
    check_version
    install_base_centos
elif  [ "${OSNAME}" == "Fedora" ]; then
    check_version
    install_base_fedora
elif  [ "${OSNAME}" == "Mac OS X" ]; then
    install_base_macos
    echo "mac"
else
    echo "This program [$0] does not support your operating system [${OSNAME}]"
    echo "Please refer to manual installation page"
fi

install_nasm
install_openssl
install_libsrtp
install_libsrt
install_libopus
install_libx264
install_libx265
install_libvpx
install_fdk_aac
install_qsv
install_nvcc
install_ffmpeg
install_jemalloc
install_libpcre2

echo ${OSNAME} ${OSVERSION}

if [ "${WITH_OME}" == "true" ]; then
    install_ovenmediaengine
fi
