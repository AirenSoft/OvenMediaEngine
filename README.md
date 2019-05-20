<a href="https://ovenmediaengine.com/">
    <img src="ome_favicon.svg" alt="OvenMediaEngine logo" title="OvenMediaEngine" align="left" height="60" />
</a>

## What is OvenMediaEngine?

OvenMediaEngine \(OME\) is an Open Source, Ultra-Low Latency Streaming Server. OME receives video via RTMP from live encoders such as OBS, XSplit and transmits it on WebRTC. So, Ultra-Low Latency Streaming from OME can work seamlessly in your browser without plug-ins. Also, OME provides [OvenPlayer](https://github.com/AirenSoft/OvenPlayer), the HTML5 standard web player.

Our goal is to make it easier for you to build a stable broadcasting/streaming service with Ultra-Low Latency.

## Features

* RTMP Input
* WebRTC/HLS/MPEG-DASH Streaming
* Embedded Live Transcoder \(VP8, H.264, Opus, AAC\)
* Embedded WebRTC Signalling Server \(WebSocket based\)
* ICE \(Interactive Connectivity Establishment\)
* DTLS \(Datagram Transport Layer Security\)
* SRTP \(Secure Real-time Transport Protocol\)
* ULPFEC \(Forward Error Correction\) with VP8, H.264
* In-band FEC \(Forward Error Correction\) with Opus
* P2P Delivery \(Preview version\)
* High Availability
* Clustering
  * Origin-Edge structure

## Supported Platforms

We have tested OME on the platforms listed below. However, we think it can work with other Linux packages as well:

* Docker
* Ubuntu 18
* CentOS 7
* Fedora 28

## Getting Started

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

