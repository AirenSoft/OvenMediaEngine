<a href="https://ovenmediaengine.com/">
    <img src="ome_favicon.svg" alt="OvenMediaEngine logo" title="OvenMediaEngine" align="left" height="60" />
</a>

## What is OvenMediaEngine?

OvenMediaEngine \(OME\) is an open source, streaming server with sub-second latency. OME receives video via RTMP or other protocols from live encoders such as OBS, XSplit and transmits it on WebRTC and Low-Latency DASH. So, sub-second latency streaming from OME can work seamlessly in your browser without plug-ins. Also, OME provides [OvenPlayer](https://github.com/AirenSoft/OvenPlayer), the HTML5 standard web player.

Our goal is to make it easier for you to build a stable broadcasting/streaming service with sub-second latency.

## Features

* RTMP Input
* WebRTC sub-second streaming 
  * ICE \(Interactive Connectivity Establishment\)
  * DTLS \(Datagram Transport Layer Security\)
  * SRTP \(Secure Real-time Transport Protocol\)
  * ULPFEC \(Forward Error Correction\) with VP8, H.264
  * In-band FEC \(Forward Error Correction\) with Opus
* Low latency MPEG-DASH(Chunked CAMF) streaming
* Legacy HLS/MPEG-DASH Streaming
* Embedded Live Transcoder \(VP8, H.264, Opus, AAC, Bypass\)
* Embedded WebRTC Signalling Server \(WebSocket based\)
* Origin-Edge structure
* Monitoring
* Experiment
  * P2P Traffic Distribution (Only WebRTC)
  * RTSP Pull, MPEG-TS/udp Push Input

## Supported Platforms

We have tested OME on the platforms listed below. However, we think it can work with other Linux packages as well:

* Docker (https://hub.docker.com/r/airensoft/ovenmediaengine)
* Ubuntu 18
* CentOS 7
* Fedora 28

## Getting Started

```
docker run -d \
-p 1935:1935 -p 3333:3333 -p 8080:8080 -p 9000:9000/udp -p 10000-10005:10000-10005/udp \
airensoft/ovenmediaengine:latest
```
Please read [Getting Started](https://airensoft.gitbook.io/ovenmediaengine/getting-started) chapter in tutorials.

## How to Contribute

Please see our [Guidelines ](CONTRIBUTING.md)and [Rules](CODE_OF_CONDUCT.md).

## For more information

* [OvenMediaEngine Website](https://ovenmediaengine.com) 
  * Basic Information, FAQ, and Benchmark
* [OvenMediaEngine Tutorials](https://airensoft.gitbook.io/ovenmediaengine/)
  * Getting Started, Install, and Configuration
* Test Player
  * `Without TLS` : [http://demo.ovenplayer.com](http://demo.ovenplayer.com)
  * `Based on TLS` : [https://demo.ovenplayer.com](https://demo.ovenplayer.com)
* [OvenPlayer Github](https://github.com/AirenSoft/OvenPlayer)
* [OvenPlayer Website](https://ovenplayer.com/index.html)
* [AirenSoft Website](https://www.airensoft.com/)

## License

OvenMediaEngine is under the [GPLv2 license](LICENSE).

