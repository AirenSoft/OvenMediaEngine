# HLS & MPEG-DASH Streaming

OvenMediaEngine \(OME\) uses HLS and MPEG-DASH to support streaming even on older browsers that do not support WebRTC. Also, if you use OvenPlayer, it detects the browser status and performance, and automatically falls back using HLS or MPEG-DASH during streaming. HLS and MPEG-DASH are not Ultra-Low Latency protocols, so you can decide whether or not to use them.

{% hint style="info" %}
We will soon support CMAF, which can stream with Low Latency even when using HLS or MPEG-DASH in OME and OvenPlayer. CMAF will work reliably on most browsers except iOS, and it can Low Latency Streaming from 1 to 3 seconds.
{% endhint %}

OME currently offers the following features:

| Title | Functions |
| :--- | :--- |
| Delivery | HTTP |
| Security | TLS \(HTTPS\) |
| Format | `TS`: MPEG-TS \(HLS\), `M4S`: ISOBMFF \(MPEG-DASH\) |
| Codec | H.264, AAC |

## Configuration

If you want to use MPEG-DASH and HLS, you need to add the `<DASH>` and `<HLS>` element to the `<Publishers>` in the `Server.xml` configuration file as shown in the following example.

{% code-tabs %}
{% code-tabs-item title="Server.xml" %}
```markup
<Publishers>
    <DASH>
        <Port>8080</Port>
        <TLS include="TLS.xml" />
        <SegmentDuration>5</SegmentDuration>
        <SegmentCount>3</SegmentCount>
        <CrossDomain>
            <Url>*<Url>
        </CrossDomain>
    </DASH>
    <HLS>
        <Port>8080</Port>
        <TLS include="TLS.xml" />
        <SegmentDuration>5</SegmentDuration>
        <SegmentCount>3</SegmentCount>
        <CrossDomain>
            <Url>https://airensoft.com<Url>
            <Url>airensoft.com<Url>
        </CrossDomain>
    </HLS>
</Publishers>
```
{% endcode-tabs-item %}
{% endcode-tabs %}

| Element | Decsription |
| :--- | :--- |
| Port | Sets the TCP `<Port>` to provide MPEG-DASH and HLS. Also, MPEG-DASH and HLS can use the same port. |
| TLS | Applies `<TLS>` to be able to access MPEG-DASH and HLS streaming URLs using HTTPS. For more information, please see the [TLS](hls-mpeg-dash.md#tls) section. |
| SegmentDuration | Sets the length of the segment in seconds. The shorter the `<SegmentDuration>`, the lower latency can be streamed, but it is less stable during streaming. Therefore, we are recommended to set it to 3 to 5 seconds. |
| SegmentCount | Sets the number of segments to be exposed to `*.m3u8` and `*.mpd`. The smaller the number of `<SegmentCount>`, the lower latency can be streamed, but it is less reliable during streaming. Therefore, it is recommended to set this value to 3. |
| CrossDomain | Controls the domain in which the player works through `<CorssDomain>`. For more information, please refer to the [CrossDomain](hls-mpeg-dash.md#crossdomain) section. |

## TLS

Most browsers prohibit using HTTP Streaming without applying TLS to the HTTPS page that has TLS applied. Therefore, if you have applied OvenPlayer to a page that has HTTPS based, `<DASH>` and `<HLS>` in OME need to also be able to connect to HTTPS with TLS applied.

```markup
<Publishers>
    <DASH>
        <TLS include="TLS.xml" />
        ...
    </DASH>
    <HLS>
        <TLS include="TLS.xml" />
        ...    
    </HLS>
</Publishers>
```

You can include other configuration files by loading them in OME as shown above. `TLS.xml` need to be in the same directory as `Server.xml`. You can set up `TLS.xml` as follows:

{% code-tabs %}
{% code-tabs-item title="TLS.xml" %}
```markup
<?xml version="1.0" encoding="UTF-8"?>
<TLS>
	<!-- certification file path for TLS servers -->
	<CertPath>/path/to/cert/filename.crt</CertPath>
	<!-- private key file path for TLS servers -->
	<KeyPath>/path/to/cert/filename.key</KeyPath>
</TLS>
```
{% endcode-tabs-item %}
{% endcode-tabs %}

When you specify the location of the `*.CRT` file and the `*.KEY` file, TLS is applied to the HTTP Server and can be accessed with `https://`.

## CrossDomain

Most browsers and players prohibit accessing other domain resources in the currently running domain. You can control this situation through Cross-Origin Resource Sharing \(CORS\) or Cross-Domain \(CrossDomain\). Moreover, OME can set CORS and Cross-Domain as `<CrossDomain>` element.

{% code-tabs %}
{% code-tabs-item title="Server.xml" %}
```markup
<CrossDomain>
    <Url>*</Url>
    <Url>airensoft.com</Url>
    <Url>http://airensoft.com</Url>
    <Url>https://airensoft.com</Url>
</CrossDomain>
```
{% endcode-tabs-item %}
{% endcode-tabs %}

You can set it using the `<Url>` element as shown above, and you can use the following values:

| Url Value | Decsription |
| :--- | :--- |
| \* | Allows requests from all Domains |
| domain | Allows both HTTP and HTTPS requests from the specified Domain |
| http://domain | Allows HTTP requests from the specified Domain |
| https://domain | Allows HTTPS requests from the specified Domain |

## Streaming

MPEG-DASH and HLS Streaming are ready when a live source is inputted and a stream is created. Viewers can stream using OvenPlayer or other players.

Also, you need to set H.264 and AAC in the Transcoding profile because MPEG-DASH and HLS use these codecs.

```markup
<Encodes>
   <Encode>
      <Name>HD_H264_AAC</Name>
      <Audio>
         <Codec>aac</Codec>
         <Bitrate>128000</Bitrate>
         <Samplerate>48000</Samplerate>
         <Channel>2</Channel>
      </Audio>
      <Video>
         <Codec>h264</Codec>
         <Width>1280</Width>
         <Height>720</Height>
         <Bitrate>2000000</Bitrate>
         <Framerate>30.0</Framerate>
      </Video>
   </Encode>
</Encodes>
<Streams>
   <Stream>
      <Name>${OriginStreamName}_o</Name>
      <Profiles>
         <Profile>HD_H264_AAC</Profile>
      </Profiles>
   </Stream>
</Streams>
```

When you create a stream as shown above, you can play MPEG-DASH and HLS through OvenPlayer with the following URL:

| Protocol | URL format |
| :--- | :--- |
| HLS | `http://<OME Server IP>[:<OME HLS Port]/<Application Name>/<Stream Name>/playlist.m3u8` |
| Secure HLS | `https://<OME Server IP>[:<OME HLS Port]/<Application Name>/<Stream Name>/playlist.m3u8` |
| MPEG-DASH | `http://<OME Server IP>[:<OME DASH Port]/<Application Name>/<Stream Name>/manifest.mpd` |
| Secure MPEG-DASH | `https://<OME Server IP>[:<OME DASH Port]/<Application Name>/<Stream Name>/manifest.mpd` |

{% hint style="info" %}
If you set TLS in the current version of OME, you can only use Secure HLS and Secure MPEG-DASH. Please wait if you want to use regular HLS and MPEG-DASH at the same time.
{% endhint %}

If you use the default configuration, you can start streaming with the following URL:

* `https://[OvenMediaEngine IP]:3333/app/stream_o/playlist.m3u8`
* `http://[OvenMediaEngine IP]:3333/app/stream_o/playlist.m3u8`
* `https://[OvenMediaEngine IP]:3333/app/stream_o/manifest.mpd`
* `http://[OvenMediaEngine IP]:3333/app/stream_o/manifest.mpd`

We have prepared a test player so that you can easily check that the OvenMediaEngine is working well. For more information, see the[ Test Player](test-player.md) chapter.

