# Getting Started

This chapter explains how to easily install OvenMediaEngine \(OME\) and check that it is working by default.

## Prerequisites

OME works in concert with a variety of open source and library. First, install them on your clean Linux machine as described below. We think that OME can support most Linux packages, but the tested platforms we use are Ubuntu 18+, Fedora 28+, and CentOS 7+.

{% tabs %}
{% tab title="Ubuntu 18" %}
#### Install packages

```text
$ sudo apt install build-essential nasm autoconf libtool zlib1g-dev libssl-dev libvpx-dev libopus-dev pkg-config libfdk-aac-dev tclsh cmake
```

#### libSRTP 2.2.0 [\[Download\]](https://github.com/cisco/libsrtp/archive/v2.2.0.tar.gz)

```text
$ (curl -OL https://github.com/cisco/libsrtp/archive/v2.2.0.tar.gz && tar xvfz v2.2.0.tar.gz && cd libsrtp-2.2.0 && ./configure --enable-openssl && make && sudo make install)
```

#### OpenH264 1.8.0 [\[Download\]](http://ciscobinary.openh264.org/libopenh264-1.8.0-linux64.4.so.bz2)

```text
$ (curl -OL http://ciscobinary.openh264.org/libopenh264-1.8.0-linux64.4.so.bz2 && bzip2 -d libopenh264-1.8.0-linux64.4.so.bz2 && sudo mv libopenh264-1.8.0-linux64.4.so /usr/lib && sudo ln -s /usr/lib/libopenh264-1.8.0-linux64.4.so /usr/lib/libopenh264.so && sudo ln -s /usr/lib/libopenh264-1.8.0-linux64.4.so /usr/lib/libopenh264.so.4)
```

#### SRT [\[Download\]](https://github.com/Haivision/srt/archive/v1.3.1.tar.gz)

```text
$ (curl -OL https://github.com/Haivision/srt/archive/v1.3.1.tar.gz && tar xvf v1.3.1.tar.gz && cd srt-1.3.1 && ./configure && make && sudo make install)
```

#### FFmpeg 3.4.2 [\[Download\]](https://www.ffmpeg.org/releases/ffmpeg-3.4.2.tar.xz)

```text
$ (curl -OL https://www.ffmpeg.org/releases/ffmpeg-3.4.2.tar.xz && xz -d ffmpeg-3.4.2.tar.xz && tar xvf ffmpeg-3.4.2.tar && cd ffmpeg-3.4.2 && ./configure \
--disable-static --enable-shared \
--extra-libs=-ldl \
--enable-ffprobe \
--disable-ffplay --disable-ffserver --disable-filters --disable-vaapi --disable-avdevice --disable-doc --disable-symver --disable-debug --disable-indevs --disable-outdevs --disable-devices --disable-hwaccels --disable-encoders \
--enable-zlib --enable-libopus --enable-libvpx --enable-libfdk_aac \
--enable-encoder=libvpx_vp8,libvpx_vp9,libopus,libfdk_aac \
--disable-decoder=tiff \
--enable-filter=asetnsamples,aresample,aformat,channelmap,channelsplit,scale,transpose,fps,settb,asettb && make && sudo make install && sudo ldconfig)
```
{% endtab %}

{% tab title="Fedora 28" %}
#### Install packages

```text
sudo yum install gcc-c++ make nasm autoconf libtool zlib-devel openssl-devel libvpx-devel opus-devel tcl cmake
```

#### Execute below commands and append them to ~/.bashrc using text editors

```text
$ export PKG_CONFIG_PATH=${PKG_CONFIG_PATH}:/usr/local/lib/pkgconfig:/usr/local/lib64/pkgconfig
$ export LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:/usr/local/lib:/usr/local/lib64
```

#### libSRTP 2.2.0 [\[Download\]](https://github.com/cisco/libsrtp/archive/v2.2.0.tar.gz)

```text
$ (curl -OL https://github.com/cisco/libsrtp/archive/v2.2.0.tar.gz && tar xvfz v2.2.0.tar.gz && cd libsrtp-2.2.0 && ./configure --enable-openssl && make && sudo make install)
```

#### FDK-AAC [\[Download\]](https://github.com/mstorsjo/fdk-aac/archive/v0.1.5.tar.gz)

```text
$ (curl -OL https://github.com/mstorsjo/fdk-aac/archive/v0.1.5.tar.gz && tar xvf v0.1.5.tar.gz && cd fdk-aac-0.1.5 && ./autogen.sh && ./configure && make && sudo make install)
```

#### FFmpeg 3.4.2 [\[Download\]](https://www.ffmpeg.org/releases/ffmpeg-3.4.2.tar.xz)

```text
$ (curl -OL https://www.ffmpeg.org/releases/ffmpeg-3.4.2.tar.xz && xz -d ffmpeg-3.4.2.tar.xz && tar xvf ffmpeg-3.4.2.tar && cd ffmpeg-3.4.2 && ./configure \
--disable-static --enable-shared \
--extra-libs=-ldl \
--enable-ffprobe \
--disable-ffplay --disable-ffserver --disable-filters --disable-vaapi --disable-avdevice --disable-doc --disable-symver --disable-debug --disable-indevs --disable-outdevs --disable-devices --disable-hwaccels --disable-encoders \
--enable-zlib --enable-libopus --enable-libvpx --enable-libfdk_aac \
--enable-encoder=libvpx_vp8,libvpx_vp9,libopus,libfdk_aac \
--disable-decoder=tiff \
--enable-filter=asetnsamples,aresample,aformat,channelmap,channelsplit,scale,transpose,fps,settb,asettb && make && sudo make install)
```

#### OpenH264 1.8.0 [\[Download\]](http://ciscobinary.openh264.org/libopenh264-1.8.0-linux64.4.so.bz2)

```text
$ (curl -OL http://ciscobinary.openh264.org/libopenh264-1.8.0-linux64.4.so.bz2 && bzip2 -d libopenh264-1.8.0-linux64.4.so.bz2 && sudo mv libopenh264-1.8.0-linux64.4.so /usr/lib && sudo ln -s /usr/lib/libopenh264-1.8.0-linux64.4.so /usr/lib/libopenh264.so && sudo ln -s /usr/lib/libopenh264-1.8.0-linux64.4.so /usr/lib/libopenh264.so.4)
```

#### SRT [\[Download\]](https://github.com/Haivision/srt/archive/v1.3.1.tar.gz)

```text
$ (curl -OL https://github.com/Haivision/srt/archive/v1.3.1.tar.gz && tar xvf v1.3.1.tar.gz && cd srt-1.3.1 && ./configure && make && sudo make install)
```
{% endtab %}

{% tab title="CentOS 7" %}
#### Install packages

```text
$ sudo yum install centos-release-scl
$ sudo yum install devtoolset-7 bc gcc-c++ nasm autoconf libtool glibc-static zlib-devel git bzip2 tcl cmake
```

#### Execute below commands and append them to ~/.bashrc using text editors

```text
$ source scl_source enable devtoolset-7
$ export LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:/usr/local/lib:/usr/local/lib64:/usr/lib
$ export PKG_CONFIG_PATH=${PKG_CONFIG_PATH}:/usr/local/lib/pkgconfig:/usr/local/lib64/pkgconfig
```

#### OpenSSL 1.1.0g [\[Download\]](https://www.openssl.org/source/openssl-1.1.0g.tar.gz)

```text
$ (curl -OL https://www.openssl.org/source/openssl-1.1.0g.tar.gz && tar xvfz openssl-1.1.0g.tar.gz && cd openssl-1.1.0g && ./config shared no-idea no-mdc2 no-rc5 no-ec2m no-ecdh no-ecdsa && make && sudo make install)
```

#### libvpx 1.7.0 [\[Download\]](https://chromium.googlesource.com/webm/libvpx/+archive/v1.7.0.tar.gz)

```text
$ (curl -OL https://chromium.googlesource.com/webm/libvpx/+archive/v1.7.0.tar.gz && mkdir libvpx-1.7.0 && cd libvpx-1.7.0 && tar xvfz ../v1.7.0.tar.gz && ./configure --enable-shared --disable-static --disable-examples && make && sudo make install)
```

#### Opus 1.1.3 [\[Download\]](https://archive.mozilla.org/pub/opus/opus-1.1.3.tar.gz)

```text
$ (curl -OL https://archive.mozilla.org/pub/opus/opus-1.1.3.tar.gz && tar xvfz opus-1.1.3.tar.gz && cd opus-1.1.3 && autoreconf -f -i && ./configure --enable-shared --disable-static && make && sudo make install)
```

#### libSRTP 2.2.0 [\[Download\]](https://github.com/cisco/libsrtp/archive/v2.2.0.tar.gz)

```text
$ (curl -OL https://github.com/cisco/libsrtp/archive/v2.2.0.tar.gz && tar xvfz v2.2.0.tar.gz && cd libsrtp-2.2.0 && ./configure --enable-openssl && make && sudo make install)
```

#### FDK-AAC [\[Download\]](https://github.com/mstorsjo/fdk-aac/archive/v0.1.5.tar.gz)

```text
$ (curl -OL https://github.com/mstorsjo/fdk-aac/archive/v0.1.5.tar.gz && tar xvf v0.1.5.tar.gz && cd fdk-aac-0.1.5 && ./autogen.sh && ./configure && make && sudo make install)
```

#### FFmpeg 3.4.2 [\[Download\]](https://www.ffmpeg.org/releases/ffmpeg-3.4.2.tar.xz)

```text
$ (curl -OL https://www.ffmpeg.org/releases/ffmpeg-3.4.2.tar.xz && xz -d ffmpeg-3.4.2.tar.xz && tar xvf ffmpeg-3.4.2.tar && cd ffmpeg-3.4.2 && ./configure \
--disable-static --enable-shared \
--extra-libs=-ldl \
--enable-ffprobe \
--disable-ffplay --disable-ffserver --disable-filters --disable-vaapi --disable-avdevice --disable-doc --disable-symver --disable-debug --disable-indevs --disable-outdevs --disable-devices --disable-hwaccels --disable-encoders \
--enable-zlib --enable-libopus --enable-libvpx --enable-libfdk_aac \
--enable-encoder=libvpx_vp8,libvpx_vp9,libopus,libfdk_aac \
--disable-decoder=tiff \
--enable-filter=asetnsamples,aresample,aformat,channelmap,channelsplit,scale,transpose,fps,settb,asettb && make && sudo make install)
```

#### OpenH264 1.8.0 [\[Download\]](http://ciscobinary.openh264.org/libopenh264-1.8.0-linux64.4.so.bz2)

```text
$ (curl -OL http://ciscobinary.openh264.org/libopenh264-1.8.0-linux64.4.so.bz2 && bzip2 -d libopenh264-1.8.0-linux64.4.so.bz2 && sudo mv libopenh264-1.8.0-linux64.4.so /usr/lib && sudo ln -s /usr/lib/libopenh264-1.8.0-linux64.4.so /usr/lib/libopenh264.so && sudo ln -s /usr/lib/libopenh264-1.8.0-linux64.4.so /usr/lib/libopenh264.so.4)
```

#### SRT [\[Download\]](https://github.com/Haivision/srt/archive/v1.3.1.tar.gz)

```text
$ (curl -OL https://github.com/Haivision/srt/archive/v1.3.1.tar.gz && tar xvf v1.3.1.tar.gz && cd srt-1.3.1 && ./configure && make && sudo make install)
```
{% endtab %}
{% endtabs %}

## **Build**

You can build OME sources using the following command:

```text
$ git clone "https://github.com/AirenSoft/OvenMediaEngine"
$ cd OvenMediaEngine/src
$ make
```

Moreover, you can find the built binaries in the `bin/DEBUG` or `bin/RELEASE` directory.

## **Configuration**

You need to create the `/conf/Server.xml` configuration file in the directory where the OME executable file resides. Moreover, OME provides a default configuration, so you can verify that it works. If you need a detailed description of the configuration, see the [Configuration](configuration.md) chapter.

```markup
$ cat conf/Server.xml
<?xml version="1.0" encoding="UTF-8"?>
<Server version="1">
    <Name>OvenMediaEngine</Name>
    <Hosts>
        <Host>
            ...
            <Applications>
                <Application>
                    <Name>app</Name>
                    <Type>live</Type>
                    ...
                    <Publishers>
                        ...
                        <WebRTC>
                            ...
```

### Ports used by default

The default configuration uses the following ports, so you need to open them in your firewall settings. 

| Port | Purpose |
| :--- | :--- |
| 1935/TCP | RTMP Input |
| 9000/UDP | Origin Listen |
| 8080/TCP | HLS & MPEG-Dash Publishing |
| 3333/TCP | WebRTC Signalling |
| 10000 - 10005/UDP | WebRTC Ice candidate |

## Run

You can run OME using the compiled binaries as follows: 

```markup
$ cd [OME_PATH]/src/bin/DEBUG
$ ./OvenMediaEngine
[04-18 16:13:51.507] I 2283 OvenMediaEngine | main.cpp:74   | OvenMediaEngine v0.1.1 (build: 18062600) is started on [getroot] (Linux x86_64 - 4.15.0-46-generic, #49-Ubuntu SMP Wed Feb 6 09:33:07 UTC 2019)
[04-18 16:13:51.507] I 2283 Config | config_manager.cpp:44   | Trying to load configurations... (/service/OvenMediaEngine/src/bin/DEBUG/conf/Server.xml)
[04-18 16:13:51.521] I 2283 OvenMediaEngine | main.cpp:118  | Trying to create application [app] (live)...
[04-18 16:13:51.521] I 2283 OvenMediaEngine | main.cpp:125  | Trying to create MediaRouter for host [default]...
[04-18 16:13:51.521] I 2283 MediaRouter | media_router.cpp:39   | Trying to start media router...
[04-18 16:13:51.522] I 2283 MediaRouter | media_router.cpp:61   | Media router is started.
[04-18 16:13:51.522] I 2283 OvenMediaEngine | main.cpp:128  | Trying to create Transcoder for host [default]...
[04-18 16:13:51.522] I 2283 OvenMediaEngine | main.cpp:135  | Trying to create RTMP Provider for application [app]...
[04-18 16:13:51.522] I 2283 OvenMediaEngine | main.cpp:159  | Trying to create SegmentStream Publisher for application [app]...
[04-18 16:13:51.524] I 2283 Publisher | publisher.cpp:12   | Trying to start publisher 4 for application: app
[04-18 16:13:51.524] I 2283 OvenMediaEngine | main.cpp:151  | Trying to create WebRTC Publisher for application [app]...
[04-18 16:13:51.525] I 2283 Publisher | publisher.cpp:12   | Trying to start publisher 1 for application: app
[04-18 16:13:51.525] I 2283 WebRTC | rtc_application.cpp:104  | WebRTC Application Started
```

We recommend that you register as a service when you use it in your service. So we plan to include an Install feature that allows you to register as a service in the official release version of OME.

## Hello Ultra-low Latency Streaming

### Start Stream

You can live streaming using live encoders such as OBS, XSplit. Please set the RTMP URL as below:

`rtmp://<OME Server IP>[:<OME RTMP Port>]/<Application name>/<Stream name>`

The meanings of each item are as follows:

* `<OME Server IP>`: It is related to the first `<IPAddress>` in the `Server.xml` file above. Usually, you can use `<IPAddress>` as is.
* `<OME RTMP Port>`: You can use `<Port>` of `<Provider>` in the above `Server.xml` file. If you use the default settings, the RTMP default port \(1935\) is used. Also, If you set the default port, you can omit the port.
* `<Application name>`: This value corresponds to `<Name>` of `<Application>` in `conf/Applications.xml` file. If you use the default settings, you can use the `app`.
* `<Stream name>`: A name that distinguishes the live stream that the publisher can determine. The determined `stream name` will affect the URL to be played later on the player side.

After you enter the above RTMP URL into the encoder and start publishing, you will have an environment in which the player can view the live stream.

### **Example of using OBS Encoder**

The transmitting address in OBS needs to use the `<Application name>` generated in `Server.xml`. If you use the default configuration, the `app` has already been created, and you can use it.

1. [ ] You install OBS on your PC and run it.
2. [ ] Click "File" in the top menu, then "Settings" \(or press "Settings" on the bottom-right\).
3. [ ] Select the "Stream" tab and fill in the settings.

![If you press the &quot;Service&quot; and select to &quot;Custom&quot;, your screen will be the same as this image.](.gitbook/assets/image%20%288%29.png)

1. [ ] Go to the "Output" tab.
2. [ ] Set the following entries.

We are strongly recommended that you use the CPU \(**ultrafast**\), Profile \(**baseline**\), and Tune \(**zerolatency**\) sections of the OBS configuration.

![If you set the &quot;Output Mode&quot; to &quot;Advanced&quot;, your screen will be the same as this image.](.gitbook/assets/image%20%2811%29.png)

### **Streaming**

Ultra-Low Latency Streaming via OME can be played on modern browsers that support WebRTC, such as Chrome, Safari, Firefox, and Edge.



When playing over WebRTC, you need a special step called Signalling. These steps are processed automatically so you can easily make it work with OvenPlayer, which implements the Signalling specification of OvenMediaEngine.

Please refer to the following source code to create an HTML page that is linked with [OvenPlayer](https://github.com/AirenSoft/OvenPlayer). If you want to know more about how to work with OvenPlayer, see [OvenPlayer Quick Start](https://github.com/AirenSoft/OvenPlayer#quick-start).

```javascript
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

Please note that the WebRTC Signalling URL in the sample code above is similar to the RTMP URL and consists of the following:

* `ws://<OME Server IP>:[<OME Signalling Port>/<Application name>/<Stream name>`
  * `<OME Server IP>`: This is related to the second `<IPAddress>` element in `Server.xml` set above. Usually, you can use the value of `<IPAddress>`.
  * `<OME Signalling Port>`: You can use the value of `<SignallingPort>` in `Server.xml` above. If you use the default settings, it will be the Signalling Default Port \(3333\).
  * `<Application name>`: Enter the value of `<Application name>` in the encoder as the value corresponding to `<Name>` in the `<Application>` in `conf/Applications.xml`.
  * `<Stream name>`: The name that distinguishes the live stream. OME uses the `_o` suffix to distinguish it from the output of `<Stream name>` in the encoder.

{% hint style="info" %}
If the RTMP publishing URL is `rtmp://192.168.0.1:1935/app/stream`, the WebRTC Signalling URL will be `ws://192.168.0.1:3333/app/stream_o`.
{% endhint %}

### **Example of using OvenPlayer**

If you are using the default configuration, you can set OvenPlayer to start broadcasting in the OBS and stream with Ultra-Low Latency as follows:

```javascript
<script>
  var webrtcSources = OvenPlayer.generateWebrtcUrls([
      {
          host : 'ws://192.168.0.225:3333',
          application : 'app',
          stream : "stream_o",
          label : "local"
  }
  ]);
  var player = OvenPlayer.create("player_id", {
      sources: webrtcSources,
  });
</script>
```

### Prepared Test Player

We have prepared a test player so that you can easily check that the OvenMediaEngine is working well. For more information, see the[ Test Player](test-player.md) chapter.

