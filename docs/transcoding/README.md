# ABR and Transcoding

OvenMediaEngine has a built-in live transcoder. The live transcoder can decode the incoming live source and re-encode it with the set codec or adjust the quality to encode at multiple bitrates.

## Supported Video, Audio and Image Codecs

OvenMediaEngine currently supports the following codecs:

| Type  | Decoder           | Encoder                     |
| ----- | ----------------- | --------------------------- |
| Video | VP8, H.264, H.265 | VP8, H.264, H.265(GPU only) |
| Audio | AAC, OPUS         | AAC, OPUS                   |
| Image |                   | JPEG, PNG                   |



## OutputProfiles

### OutputProfile

The `<OutputProfile>` setting allows incoming streams to be re-encoded via the `<Encodes>` setting to create a new output stream. The name of the new output stream is determined by the rules set in `<OutputStreamName>`, and the newly created stream can be used according to the streaming URL format.

```markup
<OutputProfiles>
    <OutputProfile>
        <Name>bypass_stream</Name>
        <OutputStreamName>${OriginStreamName}_bypass</OutputStreamName>
        <Encodes>
            <Audio>
                <Bypass>true</Bypass>
            </Audio>
            <Video>
                <Bypass>true</Bypass>
            </Video>
            <Audio>
                <Codec>opus</Codec>
                <Bitrate>128000</Bitrate>
                <Samplerate>48000</Samplerate>
                <Channel>2</Channel>
            </Audio>
        </Encodes>
    </OutputProfile>
</OutputProfiles>
```

According to the above setting, if the incoming stream name is `stream`, the output stream becomes `stream_bypass`and the stream URL can be used as follows.

* **`WebRTC`**    ws://192.168.0.1:3333/app/<mark style="background-color:blue;">stream\_bypass</mark>
* **`LLHLS`**       http://192.168.0.1:8080/app/<mark style="background-color:blue;">stream\_bypass</mark>/llhls.m3u8

### Encodes

#### Video

You can set the video profile as below:

```markup
<Encodes>
    <Video>
        <Name>720_vp8</Name>
        <Codec>vp8</Codec>
        <Width>1280</Width>
        <Height>720</Height>
        <Bitrate>2000000</Bitrate>
        <Framerate>30.0</Framerate>
        <Preset>fast</Preset>
        <ThreadCount>4</ThreadCount>
    </Video>
</Encodes>
```

The meaning of each property is as follows:

| Property                                  | Description                                 |
| ----------------------------------------- | ------------------------------------------- |
| Codec<mark style="color:red;">\*</mark>   | Specifies the `vp8` or `h264` codec to use  |
| Bitrate<mark style="color:red;">\*</mark> | Bit per second                              |
| Name                                      | Encode name for Renditions                  |
| Width                                     | Width of resolution                         |
| Height                                    | Height of resolution                        |
| Framerate                                 | Frames per second                           |
| Preset                                    | Presets of encoding quality and performance |
| ThreadCount                               | Number of threads in encoding               |

**Table of presets**

A table in which presets provided for each codec library are mapped to OvenMediaEngine's presets. Slow presets are of good quality and use a lot of resources, whereas Fast presets have lower quality and better performance. It can be set according to your own system environment and service purpose.

| Presets     | openh264         | libvpx      | h264/265 NVC | h264/265 QSV |
| ----------- | ---------------- | ----------- | ------------ | ------------ |
| **slower**  | Quantizer(10-41) | best        | hq           | -            |
| **slow**    | Quantizer(10-41) | best        | llhq         | -            |
| **medium**  | Quantizer(10-51) | good        | bd           | -            |
| **fast**    | Quantizer(25-51) | realtime    | hp           | -            |
| **faster**  | Quantizer(25-51) | \*realtime  | \*llhp       | -            |

_References_

* https://trac.ffmpeg.org/wiki/Encode/VP8
* https://docs.nvidia.com/video-technologies/video-codec-sdk/nvenc-preset-migration-guide/

#### Audio

You can set the audio profile as below:

```markup
<Encodes>
    <Audio>
        <Name>opus_128</Name>
        <Codec>opus</Codec>
        <Bitrate>128000</Bitrate>
        <Samplerate>48000</Samplerate>
        <Channel>2</Channel>
    </Audio>
</Encodes>
```

The meaning of each property is as follows:

| Property                                  | Description                                |
| ----------------------------------------- | ------------------------------------------ |
| Codec<mark style="color:red;">\*</mark>   | Specifies the `opus` or `aac` codec to use |
| Bitrate<mark style="color:red;">\*</mark> | Bits per second                            |
| Name                                      | Encode name for Renditions                 |
| Samplerate                                | Samples per second                         |
| Channel                                   | The number of audio channels               |

It is possible to have an audio only output profile by specifying the Audio profile and omitting a Video one.

#### Image

You can set the Image profile as below:

```markup
<Encodes>
    <Image>
        <Codec>jpeg</Codec>
        <Width>1280</Width>
        <Height>720</Height>
        <Framerate>1</Framerate>
    </Image>
</Encodes>
```

The meaning of each property is as follows:

| Property  | Description                                |
| --------- | ------------------------------------------ |
| Codec     | Specifies the `jpeg` or `png` codec to use |
| Width     | Width of resolution                        |
| Height    | Height of resolution                       |
| Framerate | Frames per second                          |

{% hint style="warning" %}
The image encoding profile is only used by thumbnail publishers. and, bypass option is not supported.
{% endhint %}

### Bypass without transcoding

You can configure Video and Audio to bypass transcoding as follows:

```markup
<Video>
    <Bypass>true</Bypass>
</Video>
<Audio>
    <Bypass>true</Bypass>
</Audio>
```

{% hint style="warning" %}
You need to consider codec compatibility with some browsers. For example, chrome only supports OPUS codec for audio to play WebRTC stream. If you set to bypass incoming audio, it can't play on chrome.
{% endhint %}

WebRTC doesn't support AAC, so if video bypasses transcoding, audio must be encoded in OPUS.

```markup
<Encodes>
    <Video>
        <Bypass>true</Bypass>
    </Video>
    <Audio>
        <Codec>opus</Codec>
        <Bitrate>128000</Bitrate>
        <Samplerate>48000</Samplerate>
        <Channel>2</Channel>
    </Audio>
</Encodes>
```

### **Keep the original with transcoding**

&#x20;If you want to transcode with the same quality as the original. See the sample below for possible parameters that OME supports to keep original. If you remove the **Width**, **Height**, **Framerate**, **Samplerate**, and **Channel** parameters. then, It is transcoded with the same options as the original.

```
<Encodes>
    <Video>
        <Codec>vp8</Codec>
        <Bitrate>2000000</Bitrate>
    </Video>
     <Audio>
        <Codec>opus</Codec>
        <Bitrate>128000</Bitrate>
    </Audio>  
    <Image>
        <Codec>jpeg</Codec>
    </Image>
</Encodes>
```

### Rescaling while keep the aspect ratio

To change the video resolution when transcoding, use the values of width and height in the Video encode option. If you don't know the resolution of the original, it will be difficult to keep the aspect ratio after transcoding. Please use the following methods to solve these problems. For example, if you input only the **Width** value in the Video encoding option, the **Height** value is automatically generated according to the ratio of the original video.

```
<Encodes>
    <Video>
        <Codec>h264</Codec>
        <Bitrate>2000000</Bitrate>
        <Width>1280</Width>
        <!-- Height is automatically calculated as the original video ratio -->
        <Framerate>30.0</Framerate>
    </Video>
    <Video>
        <Codec>h264</Codec>
        <Bitrate>2000000</Bitrate>
        <!-- Width is automatically calculated as the original video ratio -->        
        <Height>720</Height>
        <Framerate>30.0</Framerate>
    </Video>    
</Encodes>
```



## Adaptive Bitrates Streaming (ABR)

From version 0.14.0, OvenMediaEngine can encode same source with multiple bitrates renditions and deliver it to the player.

As shown in the example configuration below, you can provide ABR by adding `<Playlists>` to `<OutputProfile>`.  There can be multiple playlists, and each playlist can be accessed with `<FileName>`.

The method to access the playlist set through LLHLS is as follows.&#x20;

`http[s]://<domain>[:port]/<app>/<stream>/`**`<FileName>`**`.m3u8`

The method to access the Playlist set through WebRTC is as follows.&#x20;

`ws[s]://<domain>[:port]/<app>/<stream>/`**`<FileName>`**

Note that `<FileName>` must never contain the **`playlist`** and **`chunklist`** keywords. This is a reserved word used inside the system.

To set up `<Rendition>`, you need to add `<Name>` to the elements of `<Encodes>`. Connect the set `<Name>` into `<Rendition><Video>` or `<Rendition><Audio>`.&#x20;

In the example below, three quality renditions are provided and the URL to play the `abr` playlist as LLHLS is `https://domain:port/app/stream/abr.m3u8` and The WebRTC playback URL is `wss://domain:port/app/stream/abr`

```xml
<OutputProfile>
	<Name>bypass_stream</Name>
	<OutputStreamName>${OriginStreamName}</OutputStreamName>
	<!--LLHLS URL : https://domain/app/stream/abr.m3u8 --> 
	<Playlist>
		<Name>For LLHLS</Name>
		<FileName>abr</FileName>
		<Options> <!-- Optinal -->
			<!-- 
			Automatically switch rendition in WebRTC ABR 
			[Default] : true
			-->
			<WebRtcAutoAbr>true</WebRtcAutoAbr> 
		</Options>
		<Rendition>
			<Name>Bypass</Name>
			<Video>bypass_video</Video>
			<Audio>bypass_audio</Audio>
		</Rendition>
		<Rendition>
			<Name>FHD</Name>
			<Video>video_1280</Video>
			<Audio>bypass_audio</Audio>
		</Rendition>
		<Rendition>
			<Name>HD</Name>
			<Video>video_720</Video>
			<Audio>bypass_audio</Audio>
		</Rendition>
	</Playlist>
	<!--LLHLS URL : https://domain/app/stream/llhls.m3u8 --> 
	<Playlist>
		<Name>Change Default</Name>
		<FileName>llhls</FileName>
		<Rendition>
			<Name>HD</Name>
			<Video>video_720</Video>
			<Audio>bypass_audio</Audio>
		</Rendition>
	</Playlist> 
	<Encodes>
		<Audio>
			<Name>bypass_audio</Name>
			<Bypass>true</Bypass>
		</Audio>
		<Video>
			<Name>bypass_video</Name>
			<Bypass>true</Bypass>
		</Video>
		<Audio>
			<Codec>opus</Codec>
			<Bitrate>128000</Bitrate>
			<Samplerate>48000</Samplerate>
			<Channel>2</Channel>
		</Audio>
		<Video>
			<Name>video_1280</Name>
			<Codec>h264</Codec>
			<Bitrate>5024000</Bitrate>
			<Framerate>30</Framerate>
			<Width>1920</Width>
			<Height>1280</Height>
			<Preset>faster</Preset>
		</Video>
		<Video>
			<Name>video_720</Name>
			<Codec>h264</Codec>
			<Bitrate>2024000</Bitrate>
			<Framerate>30</Framerate>
			<Width>1280</Width>
			<Height>720</Height>
			<Preset>faster</Preset>
		</Video>
	</Encodes>
</OutputProfile>
```

## Supported codecs by streaming protocol

Even if you set up multiple codecs, there is a codec that matches each streaming protocol supported by OME, so it can automatically select and stream codecs that match the protocol. However, if you don't set a codec that matches the streaming protocol you want to use, it won't be streamed.

The following is a list of codecs that match each streaming protocol:

| Protocol | Supported Codec  |
| -------- | ---------------- |
| WebRTC   | VP8, H.264, Opus |
| LLHLS    | H.264, AAC       |

Therefore, you set it up as shown in the table. If you want to stream using LLHLS, you need to set up H.264 and AAC, and if you want to stream using WebRTC, you need to set up Opus.

Also, if you are going to use WebRTC on all platforms, you need to configure both VP8 and H.264. This is because different codecs are supported for each browser, for example, VP8 only, H264 only, or both.

However, don't worry. If you set the codecs correctly, OME automatically sends the stream of codecs requested by the browser.
