# OvenMediaEngine (OME)
## What is OvenMediaEngine?
OvenMediaEngine(OME) is a streaming engine for real-time live broadcasting with ultra low latency. It receives the RTMP stream from general broadcasting studio such as OBS, XSplit and transmits it on Webrtc. The streams can be played in the browser with ultra low latency without plug-ins. To make it easier to play webrtc streams in browsers, we are working on another HTML5 player project [OvenPlayer](https://github.com/AirenSoft/OvenPlayer).

Our goal is to make it easier for you d build stable real-time broadcating service easily.
 
## Features
- RTMP Input, Webrtc Output
- Live transcoding (VP8, Opus)
- Embedded WebRTC signalling server (Websocket based server)
- DTLS (Datagram Transport Layer Security)
- SRTP (Secure Real-time Transport Protocol)
- Configuration

## Supported Platforms
We already support the following platforms:
- Ubuntu 18
- Centos 7
- Fedora 28

We will support the following platforms in the future:
- macOS
- Windows

## Quick Start
### Prerequisites
- **[CentOS only]** devtoolset (gcc 6.0+ for compile, 7.0+ is recommended)
  ```
  $ sudo yum install centos-release-scl
  $ sudo yum install devtoolset-7
  
  # Execute this command
  $ source scl_source enable devtoolset-7
  # and append above command to ~/.bashrc using text editors
  ```
- **[Ubuntu only]** build-essential
  ```
  $ sudo apt install build-essential
  ```
- bc (An arbitrary precision calculator language)
  - CentOS/Fedora
    ```
    $ sudo yum install bc
    ```
  - Ubuntu
    ```
    $ sudo apt install bc
    ```
- gcc-c++
  - CentOS/Fedora
    ```
    $ sudo yum install gcc-c++
    ```
- nasm (Netwide Assembler)
  - CentOS/Fedora
    ```
    $ sudo yum install nasm
    ```
  - Ubuntu
    ```
    $ sudo apt install nasm
    ```
- autoconf
  - CentOS/Fedora
    ```
    $ sudo yum install autoconf
    ```
  - Ubuntu
    ```
    $ sudo apt install autoconf
    ```
- libtool
  - CentOS/Fedora
    ```
    $ sudo yum install libtool
    ```
  - Ubuntu
    ```
    $ sudo apt install libtool
    ```
- glibc-static
  - CentOS
    ```
    $ sudo yum install glibc-static
    ```
- zlib-devel
  - CentOS/Fedora
    ```
    $ sudo yum install zlib-devel
    ```
  - Ubuntu
    ```
    $ sudo apt install zlib1g-dev
    ```
- pkg-config
  - CentOS/Fedora
    ```
    $ sudo yum install pkg-config
    ```
  - Ubuntu
    ```
    $ sudo apt install pkg-config
    ```
- [OpenSSL 1.1.0g](https://www.openssl.org) [[Download]](https://www.openssl.org/source/openssl-1.1.0g.tar.gz)
  ```
  $ curl -OL https://www.openssl.org/source/openssl-1.1.0g.tar.gz
  $ tar xvfz openssl-1.1.0g.tar.gz
  $ cd openssl-1.1.0g
  $ ./config shared no-idea no-mdc2 no-rc5 no-ec2m no-ecdh no-ecdsa && make && make install
  ```
- [libvpx 1.7.0](https://www.webmproject.org/code/) [[Download]](https://chromium.googlesource.com/webm/libvpx/+archive/v1.7.0.tar.gz)
  ```
  $ curl -OL https://chromium.googlesource.com/webm/libvpx/+archive/v1.7.0.tar.gz
  $ mkdir libvpx-1.7.0
  $ cd libvpx-1.7.0
  $ tar xvfz ../v1.7.0.tar.gz
  $ ./configure --enable-shared --disable-static --disable-examples && make && make install
  ```
- [Opus 1.1.3](https://opus-codec.org/) [[Download]](https://archive.mozilla.org/pub/opus/opus-1.1.3.tar.gz)
  ```
  $ curl -OL https://archive.mozilla.org/pub/opus/opus-1.1.3.tar.gz
  $ tar xvfz opus-1.1.3.tar.gz
  $ cd opus-1.1.3
  $ autoreconf -f -i
  $ ./configure --enable-shared --disable-static && make && make install
  ```
- [libSRTP 2.2.0](https://github.com/cisco/libsrtp) [[Download]](https://github.com/cisco/libsrtp/archive/v2.2.0.tar.gz)
  ```
  $ curl -OL https://github.com/cisco/libsrtp/archive/v2.2.0.tar.gz
  $ tar xvfz v2.2.0.tar.gz
  $ cd libsrtp-2.2.0
  $ ./configure && make && make install
  ```
- [FFmpeg 3.4.2](https://ffmpeg.org) [[Download]](https://www.ffmpeg.org/releases/ffmpeg-3.4.2.tar.xz)
  ```
  $ curl -OL https://www.ffmpeg.org/releases/ffmpeg-3.4.2.tar.xz
  $ xz -d ffmpeg-3.4.2.tar.xz
  $ tar xvf ffmpeg-3.4.2.tar
  $ cd ffmpeg-3.4.2
  $ ./configure \
	--enable-shared \
	--disable-static \
	--extra-libs=-ldl \
	--disable-ffplay \
	--enable-ffprobe \
	--disable-ffserver \
	--disable-avdevice \
	--disable-doc \
	--disable-symver \
	--disable-debug \
	--disable-indevs \
	--disable-outdevs \
	--disable-devices \
	--disable-hwaccels \
	--disable-encoders \
	--enable-zlib \
	--disable-filters \
	--disable-vaapi \
	--enable-libopus \
	--enable-libvpx \
	--enable-encoder=libvpx_vp8,libvpx_vp9,libopus \
	--disable-decoder=tiff \
	--enable-filter=aresample,aformat,channelmap,channelsplit,scale,transpose,fps,settb,asettb && make && make install
  ```
  
### Build
You can build OME source with the following command. The built binary can be found in the ```bin/DEBUG``` or ```bin/RELEASE``` directory.
```
$ cd [OME_PATH]/src
$ make
```

### Launch
When you launch it for the first time, you must create configuration files to the location where the binary exists. The default configuration files are located at ```conf``` directory, so you can copy and use them.
```
$ cd [OME_PATH]/src/bin/DEBUG
$ cp -R ../../../docs/conf_examples conf
$ cat conf/Server.xml
<?xml version="1.0" encoding="UTF-8"?>
<Server>
        <Name>OvenMediaEngine</Name>
        <Hosts>
                <Host>
                        <Name>default</Name>
                        <!-- TODO: NEED TO CHANGE THIS IP ADDRESS -->
                        <IPAddress>127.0.0.1</IPAddress>
                        <MaxConnection>0</MaxConnection>
                        <!--
                        <WebConsole>
                                <Port>8080</Port>
                                <MaxConnection>10</MaxConnection>
                        </WebConsole>
                        <OpenAPI>
                                <Port>8081</Port>
                                <MaxConnection>10</MaxConnection>
                        </OpenAPI>
                        -->
                        <Provider>
                                <Port>1935/tcp</Port>
                                <MaxConnection>10</MaxConnection>
                        </Provider>
                        <Publisher>
                                <!-- TODO: NEED TO CHANGE THIS IP ADDRESS -->
                                <IPAddress>127.0.0.1</IPAddress>
                                <Port>1936/udp</Port>
                                <MaxConnection>10</MaxConnection>
                                <WebRTC>
                                        <!-- millisecond -->
                                        <SessionTimeout>30000</SessionTimeout>
                                        <!-- port[/protocol], port[/protocol], ... -->
                                        <CandidatePort>45050/udp</CandidatePort>
                                        <SignallingPort>3333/tcp</SignallingPort>
                                </WebRTC>
                        </Publisher>
                        <Applications-Ref>${ome.AppHome}/conf/Applications.xml</Applications-Ref>
                </Host>
        </Hosts>
</Server>
```

## How to Contribute
Please read [Guidelines](CONTRIBUTING.md) and our [Rules](CODE_OF_CONDUCT.md).

## Future works
The following features will be supported, and check the milestones for details.
- Audio support
- Various input stream
  - file, webrtc, mpeg-ts 
- Various output protocols
  - hls, mpeg-dash, ...
- Various encoding profiles
  - h.264, vp9, av1, ...
- Clustering
  - Origin-Edge architecture
- Virtual host
- Web console
- Statistics
- Recording live streams
- Webrtc extensions

## License
OvenMediaEngine is licensed under the [GPLv2 license](LICENSE).
