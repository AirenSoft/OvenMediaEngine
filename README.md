# "Sub-Second Latency Streaming Server" OvenMediaEngine

## What is OvenMediaEngine?

OvenMediaEngine (OME) is <b>Open-Source Streaming Server</b> with <b>Sub-Second Latency</b>.
OME receives <b>RTMP</b>, <b>MPEG-TS</b> (Beta), and <b>RTSP</b> (Beta) from encoders and cameras such as [OvenStreamEncoder](https://www.airensoft.com/olk), OBS, XSplit, and more. Then, it transmits media sources using <b>WebRTC</b>, <b>Low Latency MPEG-DASH</b>, <b>MPEG-DASH</b>, and <b>HLS</b>.
We also provide [OvenPlayer](https://github.com/AirenSoft/OvenPlayer), Open-Source HTML5 Player.

![main](dist/01_OvenMediaEngine.png)


## What is the goal of this project?

Our goal is to make it easier for you to build a stable broadcasting/streaming service with Sub-Second Latency.
So, our projects have the most optimized tools from Encoder to Player for smooth streaming.

Please click on each banner below for details.

[![OvenLiveKit](dist/07_OvenLiveKit.png)](https://www.airensoft.com/olk)
[![OvenMediaEngine](dist/07_OvenMediaEngine.png)](https://www.ovenmediaengine.com/ome)
[![OvenPlayer](dist/07_OvenPlayer.png)](https://www.ovenmediaengine.com/ovenplayer)


## Features

* RTMP Push, MPEG-2 TS Push (Beta), RTSP Pull (Beta) Input
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
* Beta
  * File Recording
  * RTMP Push Publishing(re-streaming)
  * Thumbnail
  * REST API
* Experiment
  * P2P Traffic Distribution (Only WebRTC)


## Supported Platforms

We have tested OME on the platforms listed below. However, we think it can work with other Linux packages as well:

* Docker (https://hub.docker.com/r/airensoft/ovenmediaengine)
* Ubuntu 18
* CentOS 7
* Fedora 28

## Getting Started

### Docker

```bash
docker run -d \ 
-p 1935:1935 \
-p 3333:3333 \
-p 8080:8080 \
-p 9000:9000 \
-p 4000-4005:4000-4005/udp \
-p 10006-10010:10006-10010/udp \
--name ovenmediaengine \
airensoft/ovenmediaengine:latest
```

You can also store the configuration files on your host:

```bash
docker run -d \
-p 1935:1935 \
-p 3333:3333 \
-p 8080:8080 \
-p 9000:9000 \
-p 4000-4005:4000-4005/udp \
-p 10006-10010:10006-10010/udp \
-v ome-origin-conf:/opt/ovenmediaengine/bin/origin_conf \
-v ome-edge-conf:/opt/ovenmediaengine/bin/edge_conf \
--name ovenmediaengine \
airensoft/ovenmediaengine:latest
```

The configuration files are now accessible under `/var/lib/docker/volumes/<volume_name>/_data`.

Following the above example, you will find them under `/var/lib/docker/volumes/ome-origin-conf/_data` and `/var/lib/docker/volumes/ome-edge-conf/_data`.

If you want them on a different location, the easiest way is to create links:

```bash
ln -s /var/lib/docker/volumes/ome-origin-conf/_data/ /my/new/path/to/ome-origin-conf \
&& ln -s /var/lib/docker/volumes/ome-edge-conf/_data/ /my/new/path/to/ome-edge-conf
```

### Other Methods

Please read [Getting Started](https://airensoft.gitbook.io/ovenmediaengine/getting-started) chapter in tutorials.


## How to Contribute

Please see our [Guidelines ](CONTRIBUTING.md)and [Rules](CODE_OF_CONDUCT.md).

And we are love to hear use cases. Please tell your story to [contact@airensoft.com](mailto:contact@airensoft.com). The voices of real-contributors are of great help to our project.

[![Contribute](dist/05_UseCases.png)](mailto:contact@airensoft.com)


## For more information

* [OvenMediaEngine Website](https://ovenmediaengine.com) 
  * Basic Information, FAQ, and Benchmark
* [OvenMediaEngine Tutorials](https://airensoft.gitbook.io/ovenmediaengine/)
  * Getting Started, Install, and Configuration
* [OvenMediaEngine Tutorials Source](https://github.com/AirenSoft/OvenMediaEngineDocs)
  * Please make a pull request for the manual in this project. Thanks for your contribution.
* Test Player
  * `Without TLS` : [http://demo.ovenplayer.com](http://demo.ovenplayer.com)
  * `Based on TLS` : [https://demo.ovenplayer.com](https://demo.ovenplayer.com)
* [OvenPlayer Github](https://github.com/AirenSoft/OvenPlayer)
* [OvenPlayer Website](https://ovenplayer.com/index.html)
* [AirenSoft Website](https://www.airensoft.com/)


## License

OvenMediaEngine is licensed under the [GPLv2](LICENSE) or later.
