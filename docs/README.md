# Introduction

## What is OvenMediaEngine?

[**OvenMediaEngine**](https://github.com/AirenSoft/OvenMediaEngine) (OME) is an **Open-Source Streaming Server** that enables **Large-Scale** and **Sub-Second Latency Live Streaming**. With OME, anyone can create services that live stream to large audiences of hundreds or more with sub-second latency and be scalable at any time, depending on the number of concurrent viewers.

OvenMediaEngine can receive a video/audio, video, or audio source from encoders and cameras such as [OvenLiveKit](https://www.ovenmediaengine.com/olk), OBS, XSplit, and more, to WebRTC, RTMP, SRT, MPEG-2 TS (Beta), or RTSP (Beta) as Input. Then, OME transmits this source using WebRTC, Low Latency MPEG-DASH (LLDASH), MPEG-DASH, and HLS as output. Also, we provide [OvenPlayer](https://github.com/AirenSoft/OvenPlayer), an Open-Source and JavaScript-based WebRTC Player for OvenMediaEngine.

Our goal is to make it easier for you to build a stable broadcasting/streaming service with sub-second latency.

## Features

* Ingest
  * WebRTC Push, RTMP Push, SRT Push, MPEG-2 TS Push, RTSP Pull
* WebRTC sub-second streaming
  * WebRTC over TCP (with embedded TURN server)
  * Embedded WebRTC Signalling Server (WebSocket based)
  * ICE (Interactive Connectivity Establishment)
  * DTLS (Datagram Transport Layer Security)
  * SRTP (Secure Real-time Transport Protocol)
  * ULPFEC (Forward Error Correction) with VP8, H.264
  * In-band FEC (Forward Error Correction) with Opus
* Low-Latency MPEG-DASH streaming (Chunked CMAF)
* Legacy HLS/MPEG-DASH streaming
* Embedded Live Transcoder (VP8, H.264, Opus, AAC, Bypass)
* Origin-Edge structure
* Monitoring
* AccessC
* Beta
  * File Recording
  * RTMP Push Publishing(re-streaming)
  * Thumbnail
  * REST API
* Experiment
  * P2P Traffic Distribution

## Supported Platforms

We have tested OvenMediaEngine on platforms, listed below. However, we think it can work with other Linux packages as well:

* Docker ([https://hub.docker.com/r/airensoft/ovenmediaengine](https://hub.docker.com/r/airensoft/ovenmediaengine))
* Ubuntu 18
* CentOS 7
* Fedora 28

## Getting Started

Please read [Getting Started](getting-started.md) chapter in tutorials.

## For more information

* [OvenMediaEngine Github](https://github.com/AirenSoft/OvenMediaEngine)
* [OvenMediaEngine Website](https://ovenmediaengine.com)&#x20;
  * Basic Information, FAQ, and Benchmark
* [OvenMediaEngine Tutorials](https://airensoft.gitbook.io/ovenmediaengine/)
  * Getting Started, Install, and Configuration
* Test Player
  * `Without TLS` : [http://demo.ovenplayer.com](http://demo.ovenplayer.com)
  * `Based on TLS` : [https://demo.ovenplayer.com](https://demo.ovenplayer.com)
* [OvenPlayer Github](https://github.com/AirenSoft/OvenPlayer)
* [OvenPlayer Website](https://ovenplayer.com/index.html)
* [AirenSoft Website](https://www.airensoft.com)

## How to Contribute

Please see our [Guidelines ](../CONTRIBUTING.md)and [Rules](../CODE\_OF\_CONDUCT.md).

## License

OvenMediaEngine is under the [GPLv2 license](https://github.com/AirenSoft/OvenMediaEngineDocs/tree/30ee3b30408d99632b4c2af88b070d9e38e201db/LICENSE/README.md).
