FROM ubuntu:18.04
#TODO: add ARGS(> v1.8)

# Install common build
RUN apt-get update && apt-get install -y build-essential \
    bc nasm autoconf libtool zlib1g-dev pkg-config curl git

# Install media packages
RUN mkdir -p /opt/ome/pkgs
WORKDIR /opt/ome/pkgs
COPY . /opt/ome/pkgs

RUN chmod +x \
    /opt/ome/pkgs/install_openssl.sh \
    /opt/ome/pkgs/install_libvpx.sh \
    /opt/ome/pkgs/install_libopus.sh \
    /opt/ome/pkgs/install_libsrtp.sh \
    /opt/ome/pkgs/install_ffmpeg.sh

RUN /opt/ome/pkgs/install_openssl.sh
RUN /opt/ome/pkgs/install_libvpx.sh
RUN /opt/ome/pkgs/install_libopus.sh
RUN /opt/ome/pkgs/install_libsrtp.sh
RUN /opt/ome/pkgs/install_ffmpeg.sh