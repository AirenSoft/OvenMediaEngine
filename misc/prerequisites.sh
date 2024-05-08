#!/bin/bash

set -x

PREFIX=/opt/ovenmediaengine
TEMP_PATH=/tmp

OME_VERSION=dev
OPENSSL_VERSION=3.0.7
SRTP_VERSION=2.4.2
SRT_VERSION=1.5.2
OPUS_VERSION=1.3.1
VPX_VERSION=1.11.0
FDKAAC_VERSION=2.0.2
NASM_VERSION=2.15.05
FFMPEG_VERSION=5.0.1
JEMALLOC_VERSION=5.3.0
PCRE2_VERSION=10.39
OPENH264_VERSION=2.4.0
HIREDIS_VERSION=1.0.2
NVCC_HDR_VERSION=11.1.5.2

INTEL_QSV_HWACCELS=false
NETINT_LOGAN_HWACCELS=false
NETINT_LOGAN_PATCH_PATH=""
NETINT_LOGAN_XCODER_COMPILE_PATH=""
NVIDIA_NV_CODEC_HWACCELS=false
XILINX_XMA_CODEC_HWACCELS=false

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

MAKEFLAGS="${MAKEFLAGS} -j${NCPU}"
CURRENT=$(pwd)
PATH=$PATH:${PREFIX}/bin

install_openssl()
{
    (DIR=${TEMP_PATH}/openssl && \
    mkdir -p ${DIR} && \
    cd ${DIR} && \
    curl -sSLf https://github.com/openssl/openssl/archive/openssl-${OPENSSL_VERSION}.tar.gz | tar -xz --strip-components=1 && \
    ./config --prefix="${PREFIX}" --openssldir="${PREFIX}" --libdir=lib -Wl,-rpath,"${PREFIX}/lib" shared no-idea no-mdc2 no-rc5 no-ec2m no-ecdh no-ecdsa no-async && \
    make -j$(nproc) && \
    sudo make install_sw && \
    rm -rf ${DIR} ) || fail_exit "openssl"
}

install_libsrtp()
{
    (DIR=${TEMP_PATH}/srtp && \
    mkdir -p ${DIR} && \
    cd ${DIR} && \
    curl -sSLf https://github.com/cisco/libsrtp/archive/v${SRTP_VERSION}.tar.gz | tar -xz --strip-components=1 && \
    ./configure --prefix="${PREFIX}" --enable-openssl --with-openssl-dir="${PREFIX}" && \
    make -j$(nproc) shared_library&& \
    sudo make install && \
    rm -rf ${DIR}) || fail_exit "srtp"
}

install_libsrt()
{
    (DIR=${TEMP_PATH}/srt && \
    mkdir -p ${DIR} && \
    cd ${DIR} && \
    curl -sSLf https://github.com/Haivision/srt/archive/v${SRT_VERSION}.tar.gz | tar -xz --strip-components=1 && \
    PKG_CONFIG_PATH=${PREFIX}/lib/pkgconfig:${PKG_CONFIG_PATH} ./configure --prefix="${PREFIX}" --enable-shared --disable-static && \
    make -j$(nproc) && \
    sudo make install && \
    rm -rf ${DIR} ) || fail_exit "srt"
}

install_libopus()
{
    (DIR=${TEMP_PATH}/opus && \
    mkdir -p ${DIR} && \
    cd ${DIR} && \
    curl -sSLf https://archive.mozilla.org/pub/opus/opus-${OPUS_VERSION}.tar.gz | tar -xz --strip-components=1 && \
    autoreconf -fiv && \
    ./configure --prefix="${PREFIX}" --enable-shared --disable-static && \
    make -j$(nproc) && \
    sudo make install && \
    sudo rm -rf ${PREFIX}/share && \
    rm -rf ${DIR}) || fail_exit "opus"
}

install_libopenh264()
{
    (DIR=${TEMP_PATH}/openh264 && \
    mkdir -p ${DIR} && \
    cd ${DIR} && \
    curl -sSLf https://github.com/cisco/openh264/archive/refs/tags/v${OPENH264_VERSION}.tar.gz | tar -xz --strip-components=1 && \
    sed -i -e "s|PREFIX=/usr/local|PREFIX=${PREFIX}|" Makefile && \
    make OS=linux && \
    sudo make install && \
    rm -rf ${DIR}) || fail_exit "openh264"

    # To build ffmpeg, build and install the downloaded openh264 source. There is no other way.
    # Download and replace the Prebuilt library for AVC/H.264 Patent
    # We send an audit to Cisco.
    # "OpenH264 Video Codec provided by Cisco Systems, Inc."

    # KERNEL=$(uname -s)
    # ARCH=$(uname -m)

    # if  [ "${KERNEL}" == "Linux" ] && [ "${ARCH}" == "x86" ]; then
    #     PLATFORM="linux32"
    # elif [ "${KERNEL}" == "Linux" ] && [ "${ARCH}" == "x86_64" ]; then
    #     PLATFORM="linux64"
    # else
    #     return
    # fi 

    # (cd ${PREFIX}/lib && \
    # FILENAME=libopenh264-${OPENH264_VERSION}-${PLATFORM}.6.so && \
    # sudo curl -O http://ciscobinary.openh264.org/${FILENAME}.bz2 && \
    # sudo bunzip2 -f ${FILENAME}.bz2 && \
    # sudo mv ${FILENAME} libopenh264.so.${OPENH264_VERSION} ) || fail_exit "prebuilt_openh264"
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
    curl -sSLf https://codeload.github.com/webmproject/libvpx/tar.gz/v${VPX_VERSION} | tar -xz --strip-components=1 && \
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
    curl -sSLf https://github.com/mstorsjo/fdk-aac/archive/v${FDKAAC_VERSION}.tar.gz | tar -xz --strip-components=1 && \
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
    curl -sSLf https://github.com/netwide-assembler/nasm/archive/refs/tags/nasm-${NASM_VERSION}.tar.gz | tar -xz --strip-components=1 && \
	./autogen.sh && \
    ./configure --prefix="${PREFIX}" && \
    make -j$(nproc) && \
	touch nasm.1 && \
	touch ndisasm.1 && \
    sudo make install && \
    rm -rf ${DIR}) || fail_exit "nasm"
}

install_nvcc_hdr() {
    if [ "$NVIDIA_NV_CODEC_HWACCELS" = true ] ; then    
        (DIR=${TEMP_PATH}/nvcc-hdr && \
        mkdir -p ${DIR} && \
        cd ${DIR} && \
        export DESTDIR=${PREFIX} && \
        curl -sSLf https://github.com/FFmpeg/nv-codec-headers/releases/download/n${NVCC_HDR_VERSION}/nv-codec-headers-${NVCC_HDR_VERSION}.tar.gz | tar -xz --strip-components=1 && sed -i 's|PREFIX.*=\(.*\)|PREFIX =|g' Makefile && \
        sudo make install ) || fail_exit "nvcc_headers"
    fi
}

install_ffmpeg()
{
    # If you need debug symbols, put the options below.
    #  --extra-cflags="-I${PREFIX}/include -g"  \
    #  --enable-debug \
    #  --disable-optimizations --disable-mmx --disable-stripping \

    if [[ -n "${EXT_FFMPEG_DISABLE}" ]]; then
        return
    fi

    # Default download url of FFmpeg
    FFMPEG_DOWNLOAD_URL="https://github.com/FFmpeg/FFmpeg/archive/refs/tags/n${FFMPEG_VERSION}.tar.gz"

    # Additional default configure options of FFmpeg
    ADDI_LICENSE=""
    ADDI_LIBS=" --disable-nvdec --disable-nvdec --disable-vaapi --disable-vdpau --disable-cuda-llvm --disable-cuvid --disable-ffnvcodec "
    ADDI_ENCODER=""
    ADDI_DECODER=""
    ADDI_HWACCEL=""
    ADDI_FILTERS=""
    ADDI_CFLAGS=""
    ADDI_LDFLAGS=""
    ADDI_EXTRA_LIBS=""

    # If there is an enable-qsv option, add libmfx
    if [ "$INTEL_QSV_HWACCELS" = true ] ; then
        ADDI_LIBS+=" --enable-libmfx "
        ADDI_ENCODER+=",h264_qsv,hevc_qsv"
        ADDI_DECODER+=",vp8_qsv,h264_qsv,hevc_qsv"
        ADDI_HWACCEL=""
        ADDI_FILTERS=""
    fi

    # If there is an enable-nvc option, add nvcodec
    if [ "$NVIDIA_NV_CODEC_HWACCELS" = true ] ; then
        ADDI_CFLAGS+="-I/usr/local/cuda/include "
        ADDI_LDFLAGS="-L/usr/local/cuda/lib64 "
        ADDI_LICENSE+=" --enable-nonfree "
        ADDI_LIBS+=" --enable-cuda-nvcc --enable-cuda-llvm --enable-libnpp --enable-nvenc --enable-nvdec --enable-ffnvcodec --enable-cuvid "
        ADDI_HWACCEL="--enable-hwaccel=cuda,cuvid "
        ADDI_ENCODER+=",h264_nvenc,hevc_nvenc"
        ADDI_DECODER+=",h264_nvdec,hevc_nvdec,h264_cuvid,hevc_cuvid"
        ADDI_FILTERS+=",scale_cuda,scale_npp,hwdownload,hwupload,hwupload_cuda"

        PATH=$PATH:/usr/local/nvidia/bin:/usr/local/cuda/bin
    fi

    # If there is an enable-xma option, add xilinx video sdk 3.0
    if [ "$XILINX_XMA_CODEC_HWACCELS" = true ] ; then
        FFMPEG_DOWNLOAD_URL=https://github.com/Xilinx/app-ffmpeg4-xma.git
        ADDI_ENCODER+=",h264_vcu_mpsoc,hevc_vcu_mpsoc"
        ADDI_DECODER+=",h264_vcu_mpsoc,hevc_vcu_mpsoc"
        ADDI_FILTERS+=",multiscale_xma,xvbm_convert"
        ADDI_LIBS+=" --enable-x86asm --enable-libxma2api --enable-libxvbm --enable-libxrm --enable-cross-compile "
        ADDI_CFLAGS+=" -I/opt/xilinx/xrt/include/xma2 "
        ADDI_LDFLAGS+="-L/opt/xilinx/xrt/lib  -Wl,-rpath,/opt/xilinx/xrt/lib -Wl,-rpath,/opt/xilinx/xrm/lib "
        ADDI_EXTRA_LIBS+="--extra-libs=-lxma2api --extra-libs=-lxrt_core --extra-libs=-lxrt_coreutil --extra-libs=-lpthread --extra-libs=-ldl "
    fi

    # Options are added by external scripts.
    if [[ -n "${EXT_FFMPEG_LICENSE}" ]]; then
        ADDI_LICENSE+=${EXT_FFMPEG_LICENSE}
    fi

    if [[ -n "${EXT_FFMPEG_LIBS}" ]] ; then
        ADDI_LIBS+=${EXT_FFMPEG_LIBS}
    fi

    if [[ -n "${EXT_FFMPEG_ENCODER}" ]] ; then
        ADDI_ENCODER+=${EXT_FFMPEG_ENCODER}
    fi

    if [[ -n "${EXT_FFMPEG_DECODER}" ]] ; then
        ADDI_DECODER+=${EXT_FFMPEG_DECODER}
    fi

    # Defining a Temporary Installation Path
    DIR=${TEMP_PATH}/ffmpeg

    # Download
    if [ "$XILINX_XMA_CODEC_HWACCELS" = false ] ; then
	    (rm -rf ${DIR}  && mkdir -p ${DIR} && \
	    cd ${DIR} && \
	    curl -sSLf ${FFMPEG_DOWNLOAD_URL} | tar -xz --strip-components=1 ) || fail_exit "ffmpeg"
    else
        # Download FFmpeg for xilinx video sdk 3.0
	    (rm -rf ${DIR}  && mkdir -p ${DIR} && \
	    git clone --depth=1 --branch U30_GA_3 https://github.com/Xilinx/app-ffmpeg4-xma.git ${DIR}) || fail_exit "ffmpeg"	
    fi
	
    # If there is an enable-nilogan option, add patch from libxcoder_logan-path 
    if [ "$NETINT_LOGAN_HWACCELS" = true ] ; then		
        echo "we are applying the patch founded in $NETINT_LOGAN_PATCH_PATH"
        patch_name=$(basename $NETINT_LOGAN_PATCH_PATH)
        cp $NETINT_LOGAN_PATCH_PATH ${DIR}		
        cd ${DIR} && patch -t -p 1 < $patch_name
        if [ "$NETINT_LOGAN_XCODER_COMPILE_PATH" != "" ] ; then
            cd $NETINT_LOGAN_XCODER_COMPILE_PATH && bash build.sh && ldconfig #the compilation of libxcoder_logan can be done before
        fi		
        ADDI_LIBS+=" --enable-libxcoder_logan --enable-ni_logan --enable-avfilter  --enable-pthreads "
        ADDI_ENCODER+=",h264_ni_logan,h265_ni_logan"
        ADDI_DECODER+=",h264_ni_logan,h265_ni_logan"
        ADDI_LICENSE+=" --enable-gpl --enable-nonfree "
        ADDI_LDFLAGS=" -lm -ldl"
        ADDI_FILTERS+=",hwdownload,hwupload,hwupload_ni_logan"
        #ADDI_EXTRA_LIBS="-lpthread"
    fi
    
    # Patch for Enterprise
    if [[ "$(type -t install_patch_ffmpeg)"  == 'function' ]];
    then
        install_patch_ffmpeg ${DIR}
    fi

    # Build & Install
    (cd ${DIR} && PKG_CONFIG_PATH=${PREFIX}/lib/pkgconfig:${PREFIX}/lib64/pkgconfig:${PREFIX}/usr/local/lib/pkgconfig:${PKG_CONFIG_PATH} ./configure \
    --prefix="${PREFIX}" \
    --extra-cflags="-I${PREFIX}/include ${ADDI_CFLAGS}"  \
    --extra-ldflags="-L${PREFIX}/lib -Wl,-rpath,${PREFIX}/lib ${ADDI_LDFLAGS}" \
    --extra-libs=-ldl ${ADDI_EXTRA_LIBS} \
    ${ADDI_LICENSE} \
    --disable-everything --disable-programs --disable-avdevice --disable-dwt --disable-lsp --disable-lzo --disable-faan --disable-pixelutils \
    --enable-shared --enable-zlib --enable-libopus --enable-libvpx --enable-libfdk_aac --enable-libopenh264 --enable-openssl --enable-network --enable-libsrt --enable-dct --enable-rdft  ${ADDI_LIBS} \
    ${ADDI_HWACCEL} \
    --enable-ffmpeg \
    --enable-encoder=libvpx_vp8,libopus,libfdk_aac,libopenh264,mjpeg,png${ADDI_ENCODER} \
    --enable-decoder=aac,aac_latm,aac_fixed,mp3float,mp3,h264,hevc,opus,vp8${ADDI_DECODER} \
    --enable-parser=aac,aac_latm,aac_fixed,h264,hevc,opus,vp8 \
    --enable-protocol=tcp,udp,rtp,file,rtmp,tls,rtmps,libsrt \
    --enable-demuxer=rtsp,flv,live_flv,mp4,mp3 \
    --enable-muxer=mp4,webm,mpegts,flv,mpjpeg \
    --enable-filter=asetnsamples,aresample,aformat,channelmap,channelsplit,scale,transpose,fps,settb,asettb,crop,format${ADDI_FILTERS} && \
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
    curl -sSLf https://github.com/jemalloc/jemalloc/releases/download/${JEMALLOC_VERSION}/jemalloc-${JEMALLOC_VERSION}.tar.bz2 | tar -jx --strip-components=1 && \
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
    curl -sSLf https://github.com/PhilipHazel/pcre2/releases/download/pcre2-${PCRE2_VERSION}/pcre2-${PCRE2_VERSION}.tar.gz | tar -xz --strip-components=1 && \
    ./configure --prefix="${PREFIX}" \
    --disable-static \
        --enable-jit=auto && \
    make -j$(nproc) && \
    sudo make install && \
    rm -rf ${DIR} ) || fail_exit "libpcre2"
}

install_hiredis()
{
	(DIR=${TEMP_PATH}/hiredis && \
    mkdir -p ${DIR} && \
    cd ${DIR} && \
    curl -sSLf https://github.com/redis/hiredis/archive/refs/tags/v${HIREDIS_VERSION}.tar.gz | tar -xz --strip-components=1 && \
    make -j$(nproc) && \
    sudo make install PREFIX="${PREFIX}" && \
    rm -rf ${DIR} ) || fail_exit "hiredis"
}

install_base_ubuntu()
{
    sudo apt-get install -y build-essential autoconf libtool zlib1g-dev tclsh cmake curl pkg-config bc uuid-dev
}

install_base_fedora()
{
    sudo yum install -y gcc-c++ make autoconf libtool zlib-devel tcl cmake bc libuuid-devel 
    sudo yum install -y perl-IPC-Cmd
}

install_base_centos()
{
    if [[ "${OSNAME}" == "CentOS" && "${OSVERSION}" == "7" ]]; then
        sudo yum install -y epel-release

        # centos-release-scl should be installed before installing devtoolset-7
        sudo yum install -y centos-release-scl
        sudo yum install -y glibc-static devtoolset-7

        source scl_source enable devtoolset-7
    elif [[ "${OSNAME}" == "Amazon Linux" && "${OSVERSION}" == "2" ]]; then
        sudo yum install -y make git which
    fi

    sudo yum install -y bc gcc-c++ autoconf libtool tcl bzip2 zlib-devel cmake libuuid-devel
    sudo yum install -y perl-IPC-Cmd
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
    curl -sSLf https://github.com/AirenSoft/OvenMediaEngine/archive/${OME_VERSION}.tar.gz | tar -xz --strip-components=1 && \
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
    if [[ "${OSNAME}" == "Ubuntu" && "${OSVERSION}" != "18" && "${OSVERSION}.${OSMINORVERSION}" != "20.04" && "${OSVERSION}.${OSMINORVERSION}" != "22.04" ]]; then
        proceed_yn
    fi

    if [[ "${OSNAME}" == "CentOS" && "${OSVERSION}" != "7" && "${OSVERSION}" != "8" ]]; then
        proceed_yn
    fi

    if [[ "${OSNAME}" == "Fedora" && "${OSVERSION}" != "28" ]]; then
        proceed_yn
    fi

	if [[ "${OSNAME}" == "Red" && "${OSVERSION}" != "8" ]]; then
        proceed_yn
    fi

	if [[ "${OSNAME}" == "Amazon Linux" && "${OSVERSION}" != "2" ]]; then
        proceed_yn
    fi
}

proceed_yn()
{
    read -p "This program [$0] is tested on [Ubuntu 18/20.04/22.04, CentOS 7/8 q, Fedora 28, Amazon Linux 2]
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
    --enable-qsv)
    INTEL_QSV_HWACCELS=true  
    shift
    ;;
	--enable-nilogan)
    NETINT_LOGAN_HWACCELS=true 	
    shift
    ;;	
	--nilogan-path=*)
    NETINT_LOGAN_PATCH_PATH="${i#*=}" 
    shift
    ;;
	--nilogan-xocder-compile-path=*)
    NETINT_LOGAN_XCODER_COMPILE_PATH="${i#*=}" 
    shift
    ;;
    --enable-nvc)
    NVIDIA_NV_CODEC_HWACCELS=true
    shift
    ;;
    --enable-xma)
    XILINX_XMA_CODEC_HWACCELS=true
    shift
    ;;    
    *)
            # unknown option
    ;;
esac
done


if [ "$NETINT_LOGAN_HWACCELS" = true ] ; then	
	if [[ ! -f $NETINT_LOGAN_PATCH_PATH ]]; then
		echo "You have activated netint logan encoding but the patch path is not found"
		exit 1
	fi	
fi


if [ "${OSNAME}" == "Ubuntu" ]; then
    check_version
    install_base_ubuntu
elif  [ "${OSNAME}" == "CentOS" ]; then
     check_version
     install_base_centos
elif  [ "${OSNAME}" == "Amazon Linux" ]; then
     check_version
     install_base_centos
elif  [ "${OSNAME}" == "Fedora" ]; then
    check_version
    install_base_fedora
elif  [ "${OSNAME}" == "Red" ]; then
    check_version
    install_base_fedora
elif  [ "${OSNAME}" == "Mac OS X" ]; then
    install_base_macos
else
    echo "This program [$0] does not support your operating system [${OSNAME}]"
    echo "Please refer to manual installation page"
fi

install_nasm
install_openssl
install_libsrtp
install_libsrt
install_libopus
install_libopenh264
install_libvpx
install_fdk_aac
install_nvcc_hdr
install_ffmpeg
install_jemalloc
install_libpcre2
install_hiredis

if [ "${WITH_OME}" == "true" ]; then
    install_ovenmediaengine
fi
