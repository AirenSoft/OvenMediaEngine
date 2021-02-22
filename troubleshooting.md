# Troubleshooting

## `prerequisites.sh` Script Failed

If you have problems with the `prerequisites.sh` script we have provided, please install it manually as follows.

### Platform Specific Installation

{% tabs %}
{% tab title="Ubuntu 18" %}
```bash
sudo apt install -y build-essential nasm autoconf libtool zlib1g-dev tclsh cmake curl
```
{% endtab %}

{% tab title="Fedora 28" %}
```bash
sudo yum install -y gcc-c++ make nasm autoconf libtool zlib-devel tcl cmake
```
{% endtab %}

{% tab title="CentOS 7" %}
```bash
# for downloading latest version of nasm (x264 needs nasm 2.13+ but centos provides 2.10 )
sudo curl -so /etc/yum.repos.d/nasm.repo https://www.nasm.us/nasm.repo
sudo yum install centos-release-scl
sudo yum install -y bc gcc-c++ cmake nasm autoconf libtool glibc-static tcl bzip2 zlib-devel devtoolset-7
source scl_source enable devtoolset-7
```
{% endtab %}
{% endtabs %}

### Common Installation 

{% code title="Install OpenSSL" %}
```bash
PREFIX=/opt/ovenmediaengine && \
OPENSSL_VERSION=1.1.0g && \
DIR=/tmp/openssl && \
mkdir -p ${DIR} && \
cd ${DIR} && \
curl -sLf https://www.openssl.org/source/openssl-${OPENSSL_VERSION}.tar.gz | tar -xz --strip-components=1 && \
./config --prefix="${PREFIX}" --openssldir="${PREFIX}" -Wl,-rpath="${PREFIX}/lib" shared no-idea no-mdc2 no-rc5 no-ec2m no-ecdh no-ecdsa no-async && \
make -j 4 && \
sudo make install_sw && \
rm -rf ${DIR} && \
sudo rm -rf ${PREFIX}/bin
```
{% endcode %}

{% code title="Install SRTP" %}
```bash
PREFIX=/opt/ovenmediaengine && \
SRTP_VERSION=2.2.0 && \
DIR=/tmp/srtp && \
mkdir -p ${DIR} && \
cd ${DIR} && \
curl -sLf https://github.com/cisco/libsrtp/archive/v${SRTP_VERSION}.tar.gz | tar -xz --strip-components=1 && \
./configure --prefix="${PREFIX}" --enable-shared --disable-static --enable-openssl --with-openssl-dir="${PREFIX}" && \
make shared_library -j 4 && \
sudo make install && \
rm -rf ${DIR}
```
{% endcode %}

{% code title="Install SRT" %}
```bash
PREFIX=/opt/ovenmediaengine && \
SRT_VERSION=1.3.3 && \
DIR=/tmp/srt && \
mkdir -p ${DIR} && \
cd ${DIR} && \
curl -sLf https://github.com/Haivision/srt/archive/v${SRT_VERSION}.tar.gz | tar -xz --strip-components=1 && \
PKG_CONFIG_PATH=${PREFIX}/lib/pkgconfig:${PKG_CONFIG_PATH} ./configure --prefix="${PREFIX}" --enable-shared --disable-static && \
make -j 4 && \
sudo make install && \
rm -rf ${DIR} && \
sudo rm -rf ${PREFIX}/bin
```
{% endcode %}

{% code title="Install Opus" %}
```bash
PREFIX=/opt/ovenmediaengine && \
OPUS_VERSION=1.1.3 && \
DIR=/tmp/opus && \
mkdir -p ${DIR} && \
cd ${DIR} && \
curl -sLf https://archive.mozilla.org/pub/opus/opus-${OPUS_VERSION}.tar.gz | tar -xz --strip-components=1 && \
autoreconf -fiv && \
./configure --prefix="${PREFIX}" --enable-shared --disable-static && \
make -j 4&& \
sudo make install && \
sudo rm -rf ${PREFIX}/share && \
rm -rf ${DIR}
```
{% endcode %}

{% code title="Install x264" %}
```bash
PREFIX=/opt/ovenmediaengine && \
X264_VERSION=20190513-2245-stable && \
DIR=/tmp/x264 && \
mkdir -p ${DIR} && \
cd ${DIR} && \
curl -sLf https://download.videolan.org/pub/videolan/x264/snapshots/x264-snapshot-${X264_VERSION}.tar.bz2 | tar -jx --strip-components=1 && \
./configure --prefix="${PREFIX}" --enable-shared --enable-pic --disable-cli && \
make -j 4&& \
sudo make install && \
rm -rf ${DIR}
```
{% endcode %}

{% code title="Install VPX" %}
```bash
PREFIX=/opt/ovenmediaengine && \
VPX_VERSION=1.7.0 && \
DIR=/tmp/vpx && \
mkdir -p ${DIR} && \
cd ${DIR} && \
curl -sLf https://codeload.github.com/webmproject/libvpx/tar.gz/v${VPX_VERSION} | tar -xz --strip-components=1 && \
./configure --prefix="${PREFIX}" --enable-vp8 --enable-pic --enable-shared --disable-static --disable-vp9 --disable-debug --disable-examples --disable-docs --disable-install-bins && \
make -j 4 && \
sudo make install && \
rm -rf ${DIR}
```
{% endcode %}

{% code title="Install FDK-AAC" %}
```bash
PREFIX=/opt/ovenmediaengine && \
FDKAAC_VERSION=0.1.5 && \
DIR=/tmp/aac && \
mkdir -p ${DIR} && \
cd ${DIR} && \
curl -sLf https://github.com/mstorsjo/fdk-aac/archive/v${FDKAAC_VERSION}.tar.gz | tar -xz --strip-components=1 && \
autoreconf -fiv && \
./configure --prefix="${PREFIX}" --enable-shared --disable-static --datadir=/tmp/aac && \
make -j 4&& \
sudo make install && \
rm -rf ${DIR}
```
{% endcode %}

{% code title="Install FFMPEG" %}
```bash
PREFIX=/opt/ovenmediaengine && \
FFMPEG_VERSION=3.4 && \
DIR=/tmp/ffmpeg && \
mkdir -p ${DIR} && \
cd ${DIR} && \
curl -sLf https://github.com/AirenSoft/FFmpeg/archive/ome/${FFMPEG_VERSION}.tar.gz | tar -xz --strip-components=1 && \
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
rm -rf ${DIR}
```
{% endcode %}

{% code title="Install JEMALLOC" %}
```bash
PREFIX=/opt/ovenmediaengine && \
JEMALLOC_VERSION=5.2.1 && \
DIR=${TEMP_PATH}/jemalloc && \
mkdir -p ${DIR} && \
cd ${DIR} && \
curl -sLf https://github.com/jemalloc/jemalloc/releases/download/${JEMALLOC_VERSION}/jemalloc-${JEMALLOC_VERSION}.tar.bz2 | tar -jx --strip-components=1 && \
./configure --prefix="${PREFIX}" && \
make && \
sudo make install_include install_lib && \
rm -rf ${DIR}
```
{% endcode %}

## **`systemctl start ovenmediaengine` failed**

### **Check SELinux** 

If SELinux is running on your system, SELinux can deny the execution of OvenMediaEngine. 

```bash
# Example of SELinux disallow OvenMediaEngine execution
$ systemctl start ovenmediaengine
==== AUTHENTICATING FOR org.freedesktop.systemd1.manage-units ====
Authentication is required to start 'ovenmediaengine.service'.
Authenticating as: Jeheon Han (getroot)
Password:
==== AUTHENTICATION COMPLETE ====
Failed to start ovenmediaengine.service: Unit ovenmediaengine. service not found.
# Check if SELinux is enabled
$ sestatus
SELinux status:                 enabled
SELinuxfs mount:                /sys/fs/selinux
SELinux root directory:         /etc/selinux
Loaded policy name:             targeted
Current mode:                   enforcing
Mode from config file:          enforcing
Policy MLS status:              enabled
Policy deny_unknown status:     allowed
Memory protection checking:     actual (secure)
Max kernel policy version:      31
# Check if SELinux denies execution
$ sudo tail /var/log/messages
...
May 17 12:44:24 localhost audit[1]: AVC avc:  denied  { read } for  pid=1 comm="systemd" name="ovenmediaengine.service" dev="dm-0" ino=16836708 scontext=system_u:system_r:init_t:s0 tcontext=system_u:object_r:default_t:s0 tclass=file permissive=0
May 17 12:44:24 localhost audit[1]: AVC avc:  denied  { read } for  pid=1 comm="systemd" name="ovenmediaengine.service" dev="dm-0" ino=16836708 scontext=system_u:system_r:init_t:s0 tcontext=system_u:object_r:default_t:s0 tclass=file permissive=0

```

You can choose between two methods of adding a policy to SELinux or setting SELinux to permissive mode. To add a policy, you must apply the SELinux policy file for the OvenMediaEngine service to your system as follows:

```bash
$ cd <OvenMediaEngine Git Clone Root Path>
$ sudo semodule -i misc/ovenmediaengine.pp
$ sudo touch /.autorelabel
# If you add a policy to SELinux, you must reboot the system.
$ sudo reboot
```

Setting SELinux to permissive mode is as simple as follows. But we don't recommend this method.

```bash
$ sudo setenforce 0
```









