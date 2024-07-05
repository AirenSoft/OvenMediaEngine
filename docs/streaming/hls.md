---
description: Beta
---

# HLS

{% hint style="warning" %}
HLS is still in development and some features such as SignedPolicy and AdmissionWebhooks are not supported.
{% endhint %}

HLS based on MPEG-2 TS containers is still useful because it provides high compatibility, including support for older devices. Therefore, OvenMediaEngine decided to officially support HLS version 7+ based on fragmented MP4 containers, called LL-HLS, as well as HLS version 3+ based on MPEG-2 TS containers.

| Title     | Descriptions                                          |
| --------- | ----------------------------------------------------- |
| Delivery  | <p>HTTP/1.1<br>HTTP/2</p>                             |
| Security  | TLS (HTTPS)                                           |
| Container | <p>MPEG-2 TS<br>(Only supports Audio/Video muxed)</p> |
| Codecs    | <p>H.264</p><p>AAC</p>                                |

{% hint style="info" %}
To differentiate between LLHLS and HLS playback addresses, we created the following rule for HLS playback addresses:

http\[s]://domain\[:port]/\<app name>/\<stream name>/<mark style="color:blue;">**ts:**</mark>\<playlist file name>.m3u8

or

http\[s]://domain\[:port]/\<app name>/\<stream name>/\<playlist file name>.m3u8?<mark style="color:blue;">**format=ts**</mark>
{% endhint %}

## Configuration

To use HLS, you need to add the `<HLS>` elements to the `<Publishers>` in the configuration as shown in the following example.

```markup
<?xml version="1.0" encoding="UTF-8"?>
<Server version="8">
  <Bind>
    <Publishers>
      <HLS>
        <Port>13333</Port>
        <TLSPort>13334</TLSPort>
        <WorkerCount>1</WorkerCount>
      </HLS>
    </Publishers>
  </Bind>
  <VirtualHosts>
    <VirtualHost>
      <Applications>
        <Application>
          <Publishers>
            <HLS>
              <SegmentCount>5</SegmentCount>
              <SegmentDuration>4</SegmentDuration>
              <DVR>
                <Enable>true</Enable>
                <EventPlaylistType>false</EventPlaylistType>
                <TempStoragePath>/tmp/ome_dvr/</TempStoragePath>
                <MaxDuration>600</MaxDuration>
              </DVR>
              <CrossDomains>
                <Url>*</Url>
              </CrossDomains>
            </HLS>
          </Publishers>
        </Application>
      </Applications>
    </VirtualHost>
  </VirtualHosts>
</Server>
```

<table><thead><tr><th width="241">Element</th><th>Decscription</th></tr></thead><tbody><tr><td>Bind</td><td>Set the HTTP ports to provide HLS.</td></tr><tr><td>SegmentDuration</td><td>Set the length of the segment in seconds. Therefore, a shorter value allows the stream to start faster. However, a value that is too short will make legacy HLS players unstable. Apple recommends <strong>6</strong> seconds for this value.</td></tr><tr><td>SegmentCount</td><td>The number of segments listed in the playlist. 5 is recommended for HLS players. Do not set below 3. It can only be used for experimentation.</td></tr><tr><td>CrossDomains</td><td>Control the domain in which the player works through <code>&#x3C;CorssDomain></code>. For more information, please refer to the <a href="broken-reference">CrossDomain</a> section.</td></tr><tr><td>DVR</td><td><p><strong>Enable</strong> <br>You can turn DVR on or off.</p><p><strong>EventPlaylistType</strong> <br>Inserts #EXT-X-PLAYLIST-TYPE: EVENT into the m3u8 file.</p><p><strong>TempStoragePath</strong> <br>Specifies a temporary folder to store old segments.</p><p><strong>MaxDuration</strong> <br>Sets the maximum duration of recorded files in milliseconds.</p></td></tr></tbody></table>

{% hint style="warning" %}
Safari Native Player only provides the Seek UI if `#EXT-X-PLAYLIST-TYPE: EVENT` is present. Since it is specified that nothing can be removed from the playlist when it is of type EVENT, you must call the [concludeHlsLive API](../rest-api/v1/virtualhost/application/stream/conclude-hls-live.md) to switch to VoD or terminate the stream before MaxDuration is exceeded if you use this option. Otherwise, unexpected behavior may occur in the Safari Player.
{% endhint %}

## Playback

HLS is ready when a live source is inputted and a stream is created. Viewers can stream using OvenPlayer or other players.

If your input stream is already h.264/aac, you can use the input stream as is like below. If not, or if you want to change the encoding quality, you can do [Transcoding](../transcoding/).

```markup
<OutputProfiles>
    <OutputProfile>
        <Name>bypass_stream</Name>
        <OutputStreamName>${OriginStreamName}</OutputStreamName>
        <Encodes>
            <Audio>
                <Bypass>true</Bypass>
            </Audio>
            <Video>
                <Bypass>true</Bypass>
            </Video>
        </Encodes>
    </OutputProfile>
</OutputProfiles>
```

HLS Publisher basically creates a `playlist.m3u8` Playlist using the first video track and the first audio track. When you create a stream, as shown above, you can play HLS with the following URL:

> http\[s]://domain\[:port]/\<app name>/\<stream name>/ts:playlist.m3u8

If you use the default configuration, you can start streaming with the following URL:

`https://domain:3334/app/<stream name>/ts:playlist.m3u8`

We have prepared a test player that you can quickly see if OvenMediaEngine is working. Please refer to the [Test Player](../quick-start/test-player.md) for more information.

## Adaptive Bitrates Streaming (ABR)

HLS can deliver adaptive bitrate streaming. OME encodes the same source with multiple renditions and delivers it to the players. And HLS Player, including OvenPlayer, selects the best quality rendition according to its network environment. Of course, these players also provide option for users to manually select rendition.

See the [Adaptive Bitrates Streaming](../transcoding/#adaptive-bitrates-streaming-abr) section for how to configure renditions.

HLS Publisher basically creates a `playlist.m3u8` Playlist using the first video track and the first audio track. If you want to create a new playlist for ABR, you can add it to Server.xml as follows:

{% hint style="info" %}
Since TS files used in HLS must have A/V pre-muxed, the Playlist must have the <mark style="color:orange;">`EnableTsPackaging`</mark> option set.
{% endhint %}

```xml
<?xml version="1.0" encoding="UTF-8"?>
<OutputProfile>
  <Name>abr_stream</Name>
  <OutputStreamName>${OriginStreamName}</OutputStreamName>
  <Playlist>
    <Name>abr</Name>
    <FileName>abr</FileName>
    <Options>
      <HLSChunklistPathDepth>0</HLSChunklistPathDepth>
      <EnableTsPackaging>true</EnableTsPackaging>
    </Options>
    <Rendition>
      <Name>SD</Name>
      <Video>video_360</Video>
      <Audio>aac_audio</Audio>
    </Rendition>
    <Rendition>
      <Name>HD</Name>
      <Video>video_720</Video>
      <Audio>aac_audio</Audio>
    </Rendition>
    <Rendition>
      <Name>FHD</Name>
      <Video>video_1080</Video>
      <Audio>aac_audio</Audio>
    </Rendition>
  </Playlist>
  <Encodes>
    <Audio>
      <Name>aac_audio</Name>
      <Codec>aac</Codec>
      <Bitrate>128000</Bitrate>
      <Samplerate>48000</Samplerate>
      <Channel>2</Channel>
      <BypassIfMatch>
        <Codec>eq</Codec>
      </BypassIfMatch>
    </Audio>
    <Video>
      <Name>video_360</Name>
      <Codec>h264</Codec>
      <Bitrate>365000</Bitrate>
      <Framerate>30</Framerate>
      <Width>640</Width>
      <Height>360</Height>
      <KeyFrameInterval>30</KeyFrameInterval>
      <ThreadCount>2</ThreadCount>
      <Preset>medium</Preset>
      <BFrames>0</BFrames>
      <ThreadCount>1</ThreadCount>
    </Video>
    <Video>
      <Name>video_720</Name>
      <Codec>h264</Codec>
      <Profile>high</Profile>
      <Bitrate>1500000</Bitrate>
      <Framerate>30</Framerate>
      <Width>1280</Width>
      <Height>720</Height>
      <KeyFrameInterval>30</KeyFrameInterval>
      <Preset>medium</Preset>
      <BFrames>2</BFrames>
      <ThreadCount>4</ThreadCount>
    </Video>
    <Video>
      <Name>video_1080</Name>
      <Codec>h264</Codec>
      <Bitrate>6000000</Bitrate>
      <Framerate>30</Framerate>
      <Width>1920</Width>
      <Height>1080</Height>
      <KeyFrameInterval>30</KeyFrameInterval>
      <ThreadCount>8</ThreadCount>
      <Preset>medium</Preset>
      <BFrames>0</BFrames>
    </Video>
  </Encodes>
</OutputProfile>
```

## CrossDomain

Most browsers and players prohibit accessing other domain resources in the currently running domain. You can control this situation through Cross-Origin Resource Sharing (CORS) or Cross-Domain (CrossDomain). You can set CORS and Cross-Domain as `<CrossDomains>` element.

{% code title="Server.xml" %}
```markup
<CrossDomains>
    <Url>*</Url>
    <Url>*.airensoft.com</Url>
    <Url>http://*.ovenplayer.com</Url>
    <Url>https://demo.ovenplayer.com</Url>
</CrossDomains>
```
{% endcode %}

You can set it using the `<Url>` element as shown above, and you can use the following values:

| Url Value      | Description                                                   |
| -------------- | ------------------------------------------------------------- |
| \*             | Allows requests from all Domains                              |
| domain         | Allows both HTTP and HTTPS requests from the specified Domain |
| http://domain  | Allows HTTP requests from the specified Domain                |
| https://domain | Allows HTTPS requests from the specified Domain               |

## Live Rewind

You can create as long a playlist as you want by setting `<DVR>` to the HLS publisher as shown below. This allows the player to rewind the live stream and play older segments. OvenMediaEngine stores and uses old segments in a file in `<DVR><TempStoragePath>` to prevent excessive memory usage. It stores as much as `<DVR><MaxDuration>` and the unit is seconds.

```xml
<HLS>
    ...
    <DVR>
        <Enable>true</Enable>
        <TempStoragePath>/tmp/ome_dvr/</TempStoragePath>
        <MaxDuration>3600</MaxDuration>
    </DVR>
    ...
</HLS>
```
