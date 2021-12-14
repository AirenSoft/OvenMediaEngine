# "Sub-Second Latency Streaming Server" OvenMediaEngine


## What is OvenMediaEngine?

In short, OvenMediaEngine (OME) is <b>Open-Source Streaming Server</b> with <b>Sub-Second Latency</b>.

OME receives a video/audio source from encoders and cameras such as [OvenStreamEncoder](https://www.airensoft.com/olk), OBS, XSplit, and more to <b>WebRTC</b>, <b>RTMP</b>, <b>SRT</b>, <b>MPEG-2 TS</b> <sup><i>Beta</sup></i>, or <b>RTSP</b> <sup><i>Beta</sup></i>.  Then, OME transmits it using <b>WebRTC</b>, <b>Low Latency MPEG-DASH</b> (LLDASH), <b>MPEG-DASH</b>, and <b>HLS</b>.

Like the picture below:
<img src="dist/01_OvenMediaEngine_210512.png" style="max-width: 100%; height: auto;">

We also provide [OvenPlayer](https://github.com/AirenSoft/OvenPlayer), Open-Source HTML5 Player that is very synergistic with OME.


## What is the goal of this project?

AirenSoft aims to make it easier for you to build a stable broadcasting/streaming service with Sub-Second Latency.
Therefore, we will continue developing and providing the most optimized tools for smooth Sub-Second Latency Streaming.

Would you please click on each link below for details:
* ["Live Streaming Encoder for Mobile" <b>OvenLiveKit SDK](https://www.airensoft.com/olk)</b>
* ["Sub-Second Latency Streaming Server" <b>OvenMediaEngine](https://www.ovenmediaengine.com/ome)</b>
* ["HTML5 Player" <b>OvenPlayer](https://www.ovenmediaengine.com/ovenplayer)</b>


## Features

* Ingest
  * Push: WebRTC, RTMP, SRT, MPEG-2 TS
  * Pull: RTSP
* Sub-Second Streaming with WebRTC
  * WebRTC over TCP (with Embedded TURN Server)
  * ICE (Interactive Connectivity Establishment)
  * DTLS (Datagram Transport Layer Security)
  * SRTP (Secure Real-time Transport Protocol)
  * ULPFEC (Uneven Level Protection Forward Error Correction)
    * <i>VP8, H.264</i>
  * In-band FEC (Forward Error Correction)
    * <i>Opus</i>
  * Embedded WebRTC Signalling Server (WebSocket based)
* Low Latency MPEG-DASH streaming (Beta)
* Legacy HLS and MPEG-DASH streaming
* Embedded Live Transcoder
  * VP8, H.264, Opus, AAC, Bypass
* Clustering (Origin-Edge structure)
* Monitoring
* Access Control
  * Adminssion Webhooks
  * SingedPolicy
* Beta
  * File Recording
  * RTMP Push Publishing (Re-streaming)
  * Thumbnail
  * REST API
* Experiment
  * P2P Traffic Distribution (Only WebRTC)


## Supported Platforms

We have tested OME on the platforms listed below. However, we think it can work with other Linux packages as well:

* [Docker](https://hub.docker.com/r/airensoft/ovenmediaengine)
* Ubuntu 18+
* CentOS 7+
* Fedora 28+


## Getting Started

### Docker

```bash
docker run -d \
-p 1935:1935 \
-p 3333:3333 \
-p 3478:3478 \
-p 8080:8080 \
-p 9000:9000 \
-p 9999:9999/udp \
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
-p 3478:3478 \
-p 8080:8080 \
-p 9000:9000 \
-p 9999:9999/udp \
-p 4000-4005:4000-4005/udp \
-p 10006-10010:10006-10010/udp \
-v ome-origin-conf:/opt/ovenmediaengine/bin/origin_conf \
-v ome-edge-conf:/opt/ovenmediaengine/bin/edge_conf \
--name ovenmediaengine \
airensoft/ovenmediaengine:latest
```

The configuration files are now accessible under `/var/lib/docker/volumes/<volume_name>/_data`.

Following the above example, you will find them under `/var/lib/docker/volumes/ome-origin-conf/_data` and `/var/lib/docker/volumes/ome-edge-conf/_data`.

If you want to put them in a different location, the easiest way is to create a link:
```bash
ln -s /var/lib/docker/volumes/ome-origin-conf/_data/ /my/new/path/to/ome-origin-conf \
&& ln -s /var/lib/docker/volumes/ome-edge-conf/_data/ /my/new/path/to/ome-edge-conf
```

#### Other Methods

Please read the [Getting Started](https://airensoft.gitbook.io/ovenmediaengine/getting-started).


## How to contribute

Thank you so much for being so interested in OvenMediaEngine.

We need your help to keep and develop our open-source project, and we want to tell you that you can contribute in many ways. Please see our [Guidelines](CONTRIBUTING.md), [Rules](CODE_OF_CONDUCT.md), and [Contribute](https://www.ovenmediaengine.com/contribute).

- [Finding Bugs](https://github.com/AirenSoft/OvenMediaEngine/blob/master/CONTRIBUTING.md#finding-bugs)
- [Reviewing Code](https://github.com/AirenSoft/OvenMediaEngine/blob/master/CONTRIBUTING.md#reviewing-code)
- [Sharing Ideas](https://github.com/AirenSoft/OvenMediaEngine/blob/master/CONTRIBUTING.md#sharing-ideas)
- [Testing](https://github.com/AirenSoft/OvenMediaEngine/blob/master/CONTRIBUTING.md#testing)
- [Improving Documentation](https://github.com/AirenSoft/OvenMediaEngine/blob/master/CONTRIBUTING.md#improving-documentation)
- [Spreading & Use Cases](https://github.com/AirenSoft/OvenMediaEngine/blob/master/CONTRIBUTING.md#spreading--use-cases)
- [Recurring Donations](https://github.com/AirenSoft/OvenMediaEngine/blob/master/CONTRIBUTING.md#recurring-donations)

We always hope that OvenMediaEngine will give you good inspiration.


## For more information

* [OvenMediaEngine Website](https://ovenmediaengine.com) 
  * Basic Information, FAQ, and Benchmark about OvenMediaEngine
* [OvenMediaEngine Tutorial](https://airensoft.gitbook.io/ovenmediaengine/)
  * Getting Started, Install, and Configuration
* [OvenMediaEngine Tutorial Source](https://github.com/AirenSoft/OvenMediaEngineDocs)
  * Please make a pull request for the manual of this project. Thanks in advance for your contribution.
* Test Player
  * `Without TLS`: [http://demo.ovenplayer.com](http://demo.ovenplayer.com)
  * `With TLS`: [https://demo.ovenplayer.com](https://demo.ovenplayer.com)
* [OvenPlayer Github](https://github.com/AirenSoft/OvenPlayer)
  * Open-Source HTML5 Player
* [AirenSoft Website](https://www.airensoft.com/)
  * AirenSoft's Solutions/Services, and Blog (Tech Journal)


## License

OvenMediaEngine is licensed under the [GPLv3](LICENSE) or later.
