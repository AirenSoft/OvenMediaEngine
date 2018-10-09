# OvenMediaEngine (OME)
## What is OvenMediaEngine?
OvenMediaEngine(OME) is a streaming engine for real-time live broadcasting with Ultra-low latency. It receives the RTMP stream from general broadcasting studios such as OBS, XSplit and transmit it on WebRTC. Video streams with Ultra-low latency can be played in a browser without plug-ins. To make it easier to play WebRTC streams in browsers, we are working on another HTML5 player project [OvenPlayer](https://github.com/AirenSoft/OvenPlayer).

Our goal is to make it easier for you to build a stable real-time broadcasting service.

## Features
- RTMP Input, Webrtc Output
- Live transcoding (VP8, Opus)
- Embedded WebRTC signalling server (Websocket based server)
- DTLS (Datagram Transport Layer Security)
- SRTP (Secure Real-time Transport Protocol)
- Configuration

## Supported Platforms
We support the following platforms:
- Ubuntu 18
- Centos 7
- Fedora 28

We will support the following platforms in the future:
- macOS
- Windows

## Quick Start
### Prerequisites
- Ubuntu
  - Install packages
  ```
  $ sudo apt install build-essential nasm autoconf libtool zlib1g-dev libssl-dev libvpx-dev libopus-dev libsrtp2-dev pkg-config libavformat-dev libavcodec-dev libavfilter-dev libswscale-dev libavresample-dev
  ```
  - [OpenH264 1.8.0](https://www.openh264.org/) [[Download]](http://ciscobinary.openh264.org/libopenh264-1.8.0-linux64.4.so.bz2)
  ```
  $ (curl -OL http://ciscobinary.openh264.org/libopenh264-1.8.0-linux64.4.so.bz2 && bzip2 -d libopenh264-1.8.0-linux64.4.so.bz2 && mv libopenh264-1.8.0-linux64.4.so /usr/lib && ln -s /usr/lib/libopenh264-1.8.0-linux64.4.so /usr/lib/libopenh264.so && ln -s /usr/lib/libopenh264-1.8.0-linux64.4.so /usr/lib/libopenh264.so.4)
  ```
- Fedora
  - Install packages
  ```
  sudo yum install gcc-c++ make nasm autoconf libtool zlib-devel openssl-devel libvpx-devel opus-devel
  ```
  - Execute below commands and append them to ~/.bashrc using text editors 
  ```
  $ export PKG_CONFIG_PATH=${PKG_CONFIG_PATH}:/usr/local/lib/pkgconfig:/usr/local/lib64/pkgconfig
  $ export LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:/usr/local/lib:/usr/local/lib64
  ```
  - [libSRTP 2.2.0](https://github.com/cisco/libsrtp) [[Download]](https://github.com/cisco/libsrtp/archive/v2.2.0.tar.gz)
  ```
  $ (curl -OL https://github.com/cisco/libsrtp/archive/v2.2.0.tar.gz && tar xvfz v2.2.0.tar.gz && cd libsrtp-2.2.0 && ./configure && make && sudo make install)
  ```
  - [FFmpeg 3.4.2](https://ffmpeg.org) [[Download]](https://www.ffmpeg.org/releases/ffmpeg-3.4.2.tar.xz)
  ```
  $ (curl -OL https://www.ffmpeg.org/releases/ffmpeg-3.4.2.tar.xz && xz -d ffmpeg-3.4.2.tar.xz && tar xvf ffmpeg-3.4.2.tar && cd ffmpeg-3.4.2 && ./configure \
	--disable-static --enable-shared \
	--extra-libs=-ldl \
	--enable-ffprobe \
	--disable-ffplay --disable-ffserver --disable-filters --disable-vaapi --disable-avdevice --disable-doc --disable-symver --disable-debug --disable-indevs --disable-outdevs --disable-devices --disable-hwaccels --disable-encoders \
	--enable-zlib --enable-libopus --enable-libvpx \
	--enable-encoder=libvpx_vp8,libvpx_vp9,libopus \
	--disable-decoder=tiff \
	--enable-filter=aresample,aformat,channelmap,channelsplit,scale,transpose,fps,settb,asettb && make && sudo make install)
  ```
  - [OpenH264 1.8.0](https://www.openh264.org/) [[Download]](http://ciscobinary.openh264.org/libopenh264-1.8.0-linux64.4.so.bz2)
  ```
  $ (curl -OL http://ciscobinary.openh264.org/libopenh264-1.8.0-linux64.4.so.bz2 && bzip2 -d libopenh264-1.8.0-linux64.4.so.bz2 && mv libopenh264-1.8.0-linux64.4.so /usr/lib && ln -s /usr/lib/libopenh264-1.8.0-linux64.4.so /usr/lib/libopenh264.so && ln -s /usr/lib/libopenh264-1.8.0-linux64.4.so /usr/lib/libopenh264.so.4)
  ```
- CentOS (gcc 7.0+ is recommended)
  - Install packages
  ```
  $ sudo yum install centos-release-scl
  $ sudo yum install devtoolset-7 bc gcc-c++ nasm autoconf libtool glibc-static zlib-devel git bzip2
  ```
  - Execute below commands and append them to ~/.bashrc using text editors 
  ```
  $ source scl_source enable devtoolset-7
  $ export LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:/usr/local/lib:/usr/local/lib64:/usr/lib
  $ export PKG_CONFIG_PATH=${PKG_CONFIG_PATH}:/usr/local/lib/pkgconfig:/usr/local/lib64/pkgconfig
  ```
  - [OpenSSL 1.1.0g](https://www.openssl.org) [[Download]](https://www.openssl.org/source/openssl-1.1.0g.tar.gz)
  ```
  $ (curl -OL https://www.openssl.org/source/openssl-1.1.0g.tar.gz && tar xvfz openssl-1.1.0g.tar.gz && cd openssl-1.1.0g && ./config shared no-idea no-mdc2 no-rc5 no-ec2m no-ecdh no-ecdsa && make && sudo make install)
  ```
  - [libvpx 1.7.0](https://www.webmproject.org/code/) [[Download]](https://chromium.googlesource.com/webm/libvpx/+archive/v1.7.0.tar.gz)
  ```
  $ (curl -OL https://chromium.googlesource.com/webm/libvpx/+archive/v1.7.0.tar.gz && mkdir libvpx-1.7.0 && cd libvpx-1.7.0 && tar xvfz ../v1.7.0.tar.gz && ./configure --enable-shared --disable-static --disable-examples && make && sudo make install)
  ```
  - [Opus 1.1.3](https://opus-codec.org/) [[Download]](https://archive.mozilla.org/pub/opus/opus-1.1.3.tar.gz)
  ```
  $ (curl -OL https://archive.mozilla.org/pub/opus/opus-1.1.3.tar.gz && tar xvfz opus-1.1.3.tar.gz && cd opus-1.1.3 && autoreconf -f -i && ./configure --enable-shared --disable-static && make && sudo make install)
  ```
  - [libSRTP 2.2.0](https://github.com/cisco/libsrtp) [[Download]](https://github.com/cisco/libsrtp/archive/v2.2.0.tar.gz)
  ```
  $ (curl -OL https://github.com/cisco/libsrtp/archive/v2.2.0.tar.gz && tar xvfz v2.2.0.tar.gz && cd libsrtp-2.2.0 && ./configure && make && sudo make install)
  ```
  - [FFmpeg 3.4.2](https://ffmpeg.org) [[Download]](https://www.ffmpeg.org/releases/ffmpeg-3.4.2.tar.xz)
  ```
  $ (curl -OL https://www.ffmpeg.org/releases/ffmpeg-3.4.2.tar.xz && xz -d ffmpeg-3.4.2.tar.xz && tar xvf ffmpeg-3.4.2.tar && cd ffmpeg-3.4.2 && ./configure \
	--disable-static --enable-shared \
	--extra-libs=-ldl \
	--enable-ffprobe \
	--disable-ffplay --disable-ffserver --disable-filters --disable-vaapi --disable-avdevice --disable-doc --disable-symver --disable-debug --disable-indevs --disable-outdevs --disable-devices --disable-hwaccels --disable-encoders \
	--enable-zlib --enable-libopus --enable-libvpx \
	--enable-encoder=libvpx_vp8,libvpx_vp9,libopus \
	--disable-decoder=tiff \
	--enable-filter=aresample,aformat,channelmap,channelsplit,scale,transpose,fps,settb,asettb && make && sudo make install)
  ```
  - [OpenH264 1.8.0](https://www.openh264.org/) [[Download]](http://ciscobinary.openh264.org/libopenh264-1.8.0-linux64.4.so.bz2)
  ```
  $ (curl -OL http://ciscobinary.openh264.org/libopenh264-1.8.0-linux64.4.so.bz2 && bzip2 -d libopenh264-1.8.0-linux64.4.so.bz2 && mv libopenh264-1.8.0-linux64.4.so /usr/lib && ln -s /usr/lib/libopenh264-1.8.0-linux64.4.so /usr/lib/libopenh264.so && ln -s /usr/lib/libopenh264-1.8.0-linux64.4.so /usr/lib/libopenh264.so.4)
  ```
  
### Build
You can build OME source with the following command. The built binary can be found in the `bin/DEBUG` or `bin/RELEASE` directory.
```
$ git clone "https://github.com/AirenSoft/OvenMediaEngine"
$ cd OvenMediaEngine/src
$ make
```

### Try it
#### Server
When you launch it for the first time, you must create configuration files to the location where the binary exists. The default configuration files are located at `conf` directory, so you can copy and use them.
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

The first `<IPAddress>` in the `Server.xml` configuration file uses the IP address to listen to the RTMP stream being published, otherwise it uses the IP of the system. If the value of this item is not set correctly, the encoder may not be connected.
The second `<IPAddress>` in `<Publisher>` is used to specify the IP address that the WebSocket server uses to listen to WebRTC Signaling, otherwise it uses the first `<IPAddress>` in the 'Server.xml' file. If the value of this item is not set correctly, playback may not be performed normally.

```
$ ./main
[07-03 12:29:20.705] I 18780 OvenMediaEngine | main.cpp:22 | OvenMediaEngine v0.1.1 (build: 18062600) is started on [Dim-Ubuntu] (Linux x86_64 - 4.15.0-23-generic, #25-Ubuntu SMP Wed May 23 18:02:16 UTC 2018)
...
```

#### Publisher
If the server is running normally, you can use an encoder such as OBS or XSplit to publish a live stream.
To publish a live stream on the encoder, you need to set the RTMP URL as it is below.

  - `rtmp://<OME Server IP>[:<OME RTMP Port>]/<Application name>/<Stream name>`

Here's what each item means:

  - `<OME Server IP>`: It is related to the first `<IPAddress>` in the `Server.xml` file above. Normally, you can use `<IPAddress>` as is.
  - `<OME RTMP Port>`: You can use `<Port>` of `<Provider>` in `Server.xml` file above. If you are using the default settings, RTMP default port (1935) is used. (If you set the default port (1935), the port can be omitted.)
  - `<Application name>`: This value corresponds to `<Name>` of `<Application>` in `conf/Applications.xml` file. If you are using the default settings, you can use `app`.
  - `<Stream name>`: Name to distinguish the live stream, which can be determined by the publisher. The determined `stream name` will affect the URL to be played later on the player side.

After you enter the above RTMP URL into the encoder and start publishing, you will have an environment in which the player can view the live stream.

#### Player
The live stream being published can be played on the latest browsers that support WebRTC, such as Chrome. When playing over WebRTC, you need a special step called Signaling. These steps are processed automatically so you can easily make it work together if you are using '[OvenPlayer](https://github.com/AirenSoft/OvenPlayer)', which implements OvenMediaEngine's Signaling specification.
Please refer to the following source code to create an HTML page that is linked with [OvenPlayer] (https://github.com/AirenSoft/OvenPlayer). When you open this HTML page in your browser, the live stream you are publishing will play. (For more information on how to work with [OvenPlayer](https://github.com/AirenSoft/OvenPlayer), please refer to [OvenPlayer Quick Start](https://github.com/AirenSoft/OvenPlayer#quick-start).)


```
<!-- import OvenPlayer css -->
<link rel="stylesheet" href="ovenplayer/css/player.css">

<!-- import OvenPlayer javascript -->
<script src="ovenplayer/ovenplayer.js"></script>

<!-- OvenPlayer will be added this area. -->
<div id="player_id"></div>
    
<script>
    // Initialize OvenPlayer.
    var player = OvenPlayer.create("player_id", {
        sources: [{type : "webrtc", file : "<WebRTC Signalling URL>", label : "1080"}]
    });
</script>
```

Please note that the WebRTC Signaling URL in the sample code above is similar to an RTMP URL and consists of the following:

  - `ws://<OME Server IP>:[<OME Signalling Port>/<Application name>/<Stream name>`
    - `<OME Server IP>`: This is related to the second `<IPAddress>` element in `Server.xml` set up above. Normally, you can use the value of `<IPAddress>`.
    - `<OME Signaling Port>`: You can use the value of `<SignalingPort>` in `Server.xml` above. If you using the default settings, it will be the Signaling Default Port (3333).
    - `<Application name>`: Enter the value of `<Application name>` in the encoder as the value corresponding to `<Name>` in the `<Application>` in `conf/Applications.xml`.
    - `<Stream name>`: This is the name to distinguish the live stream. OvenMediaEngine uses the `_o` suffix to distinguish it from the output of `<Stream name>` in the encoder.

For example, if the RTMP URL is `rtmp://192.168.0.1:1935/app/stream`, the WebRTC Signaling URL will be `ws://192.168.0.1:3333/app/stream_o`.

## How to Contribute
Please read [Guidelines](CONTRIBUTING.md) and our [Rules](CODE_OF_CONDUCT.md).

## Future works
The following features will be supported, and check the milestones for more details.
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
