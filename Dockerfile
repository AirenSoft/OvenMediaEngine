FROM    ubuntu:20.04 AS base

## Install libraries by package
ENV     DEBIAN_FRONTEND=noninteractive
RUN     apt-get update && apt-get install -y tzdata

FROM    base AS build

WORKDIR /tmp

ARG     PREFIX=/opt/ovenmediaengine
ARG     MAKEFLAGS="-j16"

ENV     OME_VERSION=master \
        OPENSSL_VERSION=3.0.0-beta1 \
        SRTP_VERSION=2.4.0 \
        SRT_VERSION=1.4.2 \
        OPUS_VERSION=1.1.3 \
        X264_VERSION=20190513-2245-stable \
        X265_VERSION=3.4 \
        VPX_VERSION=1.7.0 \
        FDKAAC_VERSION=0.1.5 \
        FFMPEG_VERSION=4.3.2 \
        JEMALLOC_VERSION=5.2.1 \
        PCRE2_VERSION=10.39 

## Install build utils
RUN     apt-get -y install build-essential nasm autoconf libtool zlib1g-dev tclsh cmake curl pkg-config bc uuid-dev

## Build OpenSSL
RUN \
        DIR=/tmp/openssl && \
        mkdir -p ${DIR} && \
        cd ${DIR} && \
        curl -sLf https://github.com/openssl/openssl/archive/openssl-${OPENSSL_VERSION}.tar.gz | tar -xz --strip-components=1 && \
        ./config --prefix="${PREFIX}" --openssldir="${PREFIX}" -Wl,-rpath="${PREFIX}/lib" shared no-idea no-mdc2 no-rc5 no-ec2m no-ecdh no-ecdsa no-async && \
        make && \
        make install_sw && \
        rm -rf ${DIR} && \
        rm -rf ${PREFIX}/bin

## Build SRTP
RUN \
        DIR=/tmp/srtp && \
        mkdir -p ${DIR} && \
        cd ${DIR} && \
        curl -sLf https://github.com/cisco/libsrtp/archive/v${SRTP_VERSION}.tar.gz | tar -xz --strip-components=1 && \
        ./configure --prefix="${PREFIX}" --enable-shared --disable-static --enable-openssl --with-openssl-dir="${PREFIX}" && \
        make shared_library && \
        make install && \
        rm -rf ${DIR}

## Build SRT
RUN \
        DIR=/tmp/srt && \
        mkdir -p ${DIR} && \
        cd ${DIR} && \
        curl -sLf https://github.com/Haivision/srt/archive/v${SRT_VERSION}.tar.gz | tar -xz --strip-components=1 && \
        PKG_CONFIG_PATH="${PREFIX}/lib/pkgconfig:${PKG_CONFIG_PATH}" ./configure \
        --prefix="${PREFIX}" --enable-shared --disable-static && \
        make && \
        make install && \
        rm -rf ${DIR} && \
        rm -rf ${PREFIX}/bin

## Build OPUS
RUN \
        DIR=/tmp/opus && \
        mkdir -p ${DIR} && \
        cd ${DIR} && \
        curl -sLf https://archive.mozilla.org/pub/opus/opus-${OPUS_VERSION}.tar.gz | tar -xz --strip-components=1 && \
        autoreconf -fiv && \
        ./configure --prefix="${PREFIX}" --enable-shared --disable-static && \
        make && \
        make install && \
        rm -rf ${PREFIX}/share && \
        rm -rf ${DIR}

## Build X264
RUN \
        DIR=/tmp/x264 && \
        mkdir -p ${DIR} && \
        cd ${DIR} && \
        curl -sLf https://download.videolan.org/pub/videolan/x264/snapshots/x264-snapshot-${X264_VERSION}.tar.bz2 | tar -jx --strip-components=1 && \
        ./configure --prefix="${PREFIX}" --enable-shared --enable-pic --disable-cli && \
        make && \
        make install && \
        rm -rf ${DIR}

## Build X265
RUN \
        DIR=/tmp/x265 && \
        mkdir -p ${DIR} && \
        cd ${DIR} && \
        curl -sLf https://github.com/videolan/x265/archive/${X265_VERSION}.tar.gz | tar -zx --strip-components=1 && \
        cd ${DIR}/build/linux && \
        cmake -G "Unix Makefiles" -DCMAKE_INSTALL_PREFIX="${PREFIX}" -DENABLE_SHARED:bool=on ../../source && \
        make && \
        make install && \
        rm -rf ${DIR}

## Build VPX
RUN \
        DIR=/tmp/vpx && \
        mkdir -p ${DIR} && \
        cd ${DIR} && \
        curl -sLf https://codeload.github.com/webmproject/libvpx/tar.gz/v${VPX_VERSION} | tar -xz --strip-components=1 && \
        ./configure --prefix="${PREFIX}" --enable-vp8 --enable-pic --enable-shared --disable-static --disable-vp9 --disable-debug --disable-examples --disable-docs --disable-install-bins && \
        make && \
        make install && \
        rm -rf ${DIR}

## Build FDK-AAC
RUN \
        DIR=/tmp/aac && \
        mkdir -p ${DIR} && \
        cd ${DIR} && \
        curl -sLf https://github.com/mstorsjo/fdk-aac/archive/v${FDKAAC_VERSION}.tar.gz | tar -xz --strip-components=1 && \
        autoreconf -fiv && \
        ./configure --prefix="${PREFIX}" --enable-shared --disable-static --datadir=/tmp/aac && \
        make && \
        make install && \
        rm -rf ${DIR}

## Build FFMPEG
RUN \
        DIR=/tmp/ffmpeg && \
        mkdir -p ${DIR} && \
        cd ${DIR} && \
        curl -sLf https://github.com/FFmpeg/FFmpeg/archive/refs/tags/n${FFMPEG_VERSION}.tar.gz | tar -xz --strip-components=1 && \
        PKG_CONFIG_PATH="${PREFIX}/lib/pkgconfig:${PKG_CONFIG_PATH}" ./configure \
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
        --disable-avdevice --disable-dct --disable-dwt --disable-lsp --disable-lzo --disable-rdft --disable-faan --disable-pixelutils \
        --enable-zlib --enable-libopus --enable-libvpx --enable-libfdk_aac --enable-libx264 --enable-libx265 \
        --disable-everything \
        --enable-encoder=libvpx_vp8,libvpx_vp9,libopus,libfdk_aac,libx264,libx265,mjpeg,png \
        --enable-decoder=aac,aac_latm,aac_fixed,h264,hevc,opus,vp8 \
        --enable-parser=aac,aac_latm,aac_fixed,h264,hevc,opus,vp8 \        
        --enable-network --enable-protocol=tcp --enable-protocol=udp --enable-protocol=rtp,file,rtmp --enable-demuxer=rtsp --enable-muxer=mp4,webm,mpegts,flv,mpjpeg \
        --enable-filter=asetnsamples,aresample,aformat,channelmap,channelsplit,scale,transpose,fps,settb,asettb,format && \
        make && \
        make install && \
        rm -rf ${PREFIX}/share && \
        rm -rf ${DIR}

## Build jemalloc
RUN \
        DIR=/tmp/jemalloc && \
        mkdir -p ${DIR} && \
        cd ${DIR} && \
        curl -sLf https://github.com/jemalloc/jemalloc/releases/download/${JEMALLOC_VERSION}/jemalloc-${JEMALLOC_VERSION}.tar.bz2 | tar -jx --strip-components=1 && \
        ./configure --prefix="${PREFIX}" && \
        make && \
        make install_include install_lib && \
        rm -rf ${DIR}

## Build pcre2
RUN \
        DIR=/tmp/libpcre2 && \
        mkdir -p ${DIR} && \
        cd ${DIR} && \
        curl -sLf https://github.com/PhilipHazel/pcre2/releases/download/pcre2-${PCRE2_VERSION}/pcre2-${PCRE2_VERSION}.tar.gz | tar -xz --strip-components=1 && \
        ./configure --prefix="${PREFIX}" \
        --disable-static \
        --enable-jit=auto && \
        make && \
        make install && \
        rm -rf ${DIR} && \
        rm -rf ${PREFIX}/bin

## Build OvenMediaEngine
RUN \
        DIR=/tmp/ome && \
        mkdir -p ${DIR} && \
        cd ${DIR} && \
        curl -sLf https://github.com/AirenSoft/OvenMediaEngine/archive/${OME_VERSION}.tar.gz | tar -xz --strip-components=1 && \
        cd src && \
        make release

## Make running environment
RUN \
        DIR=/tmp/ome && \
        cd ${DIR} && \
        cd src && \
        mkdir -p ${PREFIX}/bin/origin_conf && \
        mkdir -p ${PREFIX}/bin/edge_conf && \
        strip ./bin/RELEASE/OvenMediaEngine && \
        cp ./bin/RELEASE/OvenMediaEngine ${PREFIX}/bin/ && \
        cp ../misc/conf_examples/Origin.xml ${PREFIX}/bin/origin_conf/Server.xml && \
        cp ../misc/conf_examples/Logger.xml ${PREFIX}/bin/origin_conf/Logger.xml && \
        cp ../misc/conf_examples/Edge.xml ${PREFIX}/bin/edge_conf/Server.xml && \
        cp ../misc/conf_examples/Logger.xml ${PREFIX}/bin/edge_conf/Logger.xml && \
        rm -rf ${DIR}

FROM	base AS release
MAINTAINER  Jeheon Han <getroot@airensoft.com>

WORKDIR         /opt/ovenmediaengine/bin
EXPOSE          80/tcp 8080/tcp 8090/tcp 1935/tcp 3333/tcp 3334/tcp 4000-4005/udp 10000-10010/udp 9000/tcp
COPY            --from=build /opt/ovenmediaengine /opt/ovenmediaengine
# Default run as Origin mode
CMD             ["/opt/ovenmediaengine/bin/OvenMediaEngine", "-c", "origin_conf"]
