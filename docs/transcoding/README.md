# ABR and Transcoding

OvenMediaEngine has a built-in live transcoder. The live transcoder can decode the incoming live source and re-encode it with the set codec or adjust the quality to encode at multiple bitrates.

## Supported Video, Audio and Image Codecs

### Decoders

<table><thead><tr><th width="179.33333333333331">Type</th><th width="544">Codec</th></tr></thead><tbody><tr><td>Video</td><td>VP8, H.264, H.265</td></tr><tr><td>Audio</td><td>AAC, Opus</td></tr></tbody></table>

### Encoders

<table><thead><tr><th width="184">Type</th><th width="177.33333333333331">Codec</th><th>Codec of Configuration</th></tr></thead><tbody><tr><td>Video</td><td>VP8</td><td>vp8</td></tr><tr><td></td><td>H.264</td><td>h264  <em><mark style="color:blue;">(Automatic Codec Selection)</mark></em></td></tr><tr><td></td><td>Open H264</td><td>h264_openh264</td></tr><tr><td></td><td>Nvidia Hardware</td><td>h264_nvenc</td></tr><tr><td></td><td>Intel Hardware</td><td>h264_qsv</td></tr><tr><td></td><td>Xilinx Hardware</td><td>h264_XMA</td></tr><tr><td></td><td>NetInt Hardware</td><td>h264_NILOGAN</td></tr><tr><td></td><td>H.265</td><td>h265 <em><mark style="color:blue;">(Automatic Codec Selection)</mark></em></td></tr><tr><td></td><td>Nvidia Hardware</td><td>h265_nvenc</td></tr><tr><td></td><td>Intel Hardware</td><td>h265_qsv</td></tr><tr><td></td><td>Xilinx Hardware</td><td>h265_xma</td></tr><tr><td></td><td>NetInt Hardware</td><td>h265_nilogan</td></tr><tr><td>Audio</td><td>AAC</td><td>aac</td></tr><tr><td></td><td>Opus</td><td>opus</td></tr><tr><td>Image</td><td>JPEG</td><td>jpeg</td></tr><tr><td></td><td>PNG</td><td>png</td></tr></tbody></table>



## OutputProfiles

### OutputProfile

The `<OutputProfile>` setting allows incoming streams to be re-encoded via the `<Encodes>` setting to create a new output stream. The name of the new output stream is determined by the rules set in `<OutputStreamName>`, and the newly created stream can be used according to the streaming URL format.

```markup
<OutputProfiles>
    <OutputProfile>
        <Name>bypass_stream</Name>
        <OutputStreamName>${OriginStreamName}_bypass</OutputStreamName>
        <Encodes>
            <Video>
                <Bypass>true</Bypass>
            </Video>
            <Audio>
                <Bypass>true</Bypass>
            </Audio>            
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

* **`WebRTC`** ws://192.168.0.1:3333/app/<mark style="background-color:blue;">stream\_bypass</mark>
* **`LLHLS`** http://192.168.0.1:8080/app/<mark style="background-color:blue;">stream\_bypass</mark>/llhls.m3u8
* **`HLS`** http://192.168.0.1:8080/app/<mark style="background-color:blue;">stream\_bypass</mark>/ts:playlist.m3u8

### Encodes

#### Video

You can set the video profile as below:

```markup
<Encodes>
    <Video>
        <Name>h264_hd</Name>
        <Codec>h264</Codec>
        <Width>1280</Width>
        <Height>720</Height>
        <Bitrate>2000000</Bitrate>
        <Framerate>30.0</Framerate>
        <KeyFrameInterval>30</KeyFrameInterval>
        <BFrames>0</BFrames>
        <Preset>fast</Preset>
        <ThreadCount>4</ThreadCount>
    </Video>
</Encodes>
```

The meaning of each property is as follows:

<table><thead><tr><th width="238">Property</th><th>Description</th></tr></thead><tbody><tr><td>Codec<mark style="color:red;">*</mark></td><td>Specifies the <code>vp8</code> or <code>h264</code> codec to use</td></tr><tr><td>Bitrate<mark style="color:red;">*</mark></td><td>Bit per second</td></tr><tr><td>Name</td><td>Encode name for Renditions</td></tr><tr><td>Width</td><td>Width of resolution</td></tr><tr><td>Height</td><td>Height of resolution</td></tr><tr><td>Framerate</td><td>Frames per second</td></tr><tr><td>KeyFrameInterval</td><td>Number of frames between two keyframes (0~600)<br><mark style="color:blue;">default is framerate (i.e. 1 second)</mark></td></tr><tr><td>BFrames</td><td>Number of B-frame (0~16)<br><mark style="color:blue;">default is 0</mark></td></tr><tr><td>Profile</td><td>H264 only encoding profile (baseline, main, high)</td></tr><tr><td>Preset</td><td>Presets of encoding quality and performance</td></tr><tr><td>ThreadCount</td><td>Number of threads in encoding</td></tr></tbody></table>

&#x20;<mark style="color:red;">\*</mark> required



**Table of presets**

A table in which presets provided for each codec library are mapped to OvenMediaEngine presets. Slow presets are of good quality and use a lot of resources, whereas Fast presets have lower quality and better performance. It can be set according to your own system environment and service purpose.

<table><thead><tr><th width="133">Presets</th><th width="138">openh264</th><th width="173">h264_nvenc</th><th width="151">h264_qsv</th><th width="122">vp8</th></tr></thead><tbody><tr><td><strong>slower</strong></td><td>QP( 10-39)</td><td>p7</td><td>No Support</td><td>best</td></tr><tr><td><strong>slow</strong></td><td>QP (16-45)</td><td>p6</td><td>No Support</td><td>best</td></tr><tr><td><strong>medium</strong></td><td>QP (24-51)</td><td>p5</td><td>No Support</td><td>good</td></tr><tr><td><strong>fast</strong></td><td>QP (32-51)</td><td>p4</td><td>No Support</td><td>realtime</td></tr><tr><td><strong>faster</strong></td><td>QP (40-51)</td><td>p3</td><td>No Support</td><td>realtime</td></tr></tbody></table>

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

&#x20;<mark style="color:red;">\*</mark> required



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

### **Conditional transcoding**

If the codec or quality of the input stream is the same as the profile to be encoded into the output stream. there is no need to perform re-transcoding while unnecessarily consuming a lot of system resources. If the quality of the input track matches all the conditions of **BypassIfMatch**, it will be Pass-through without encoding

#### Matching elements in video

<table><thead><tr><th width="206">Elements</th><th width="166">Condition</th><th>Description</th></tr></thead><tbody><tr><td>Codec <em><mark style="color:blue;">(Optional)</mark></em></td><td>eq</td><td>Compare video codecs</td></tr><tr><td>Width <em><mark style="color:blue;">(Optional)</mark></em></td><td>eq, lte, gte</td><td>Compare horizontal pixel of video resolution</td></tr><tr><td>Height <em><mark style="color:blue;">(Optional)</mark></em></td><td>eq, lte, gte</td><td>Compare vertical pixel of video resolution</td></tr><tr><td>SAR <em><mark style="color:blue;">(Optional)</mark></em></td><td>eq</td><td>Compare ratio of video resolution</td></tr></tbody></table>

&#x20;  \* **eq**: equal to / **lte**: less than or equal to / **gte**: greater than or equal to

#### Matching elements in audio

<table><thead><tr><th width="214">Elements</th><th width="162">Condition</th><th>Description</th></tr></thead><tbody><tr><td>Codec <em><mark style="color:blue;">(Optional)</mark></em></td><td>eq</td><td>Compare audio codecs</td></tr><tr><td>Samplerate <em><mark style="color:blue;">(Optional)</mark></em></td><td>eq, lte, gte</td><td>Compare sampling rate of audio</td></tr><tr><td>Channel <em><mark style="color:blue;">(Optional)</mark></em></td><td>eq, lte, gte</td><td>Compare number of channels in audio</td></tr></tbody></table>

&#x20;  \* **eq**: equal to / **lte**: less than or equal to / **gte**: greater than or equal to



To support WebRTC and LLHLS, AAC and Opus codecs must be supported at the same time. Use the settings below to reduce unnecessary audio encoding.

```xml
<Encodes>
	<Video>
                <Bypass>true</Bypass>	
	</Video>
	<Audio>
		<Name>cond_audio_aac</Name>
		<Codec>aac</Codec>
		<Bitrate>128000</Bitrate>
		<Samplerate>48000</Samplerate>
		<Channel>2</Channel>
		<BypassIfMatch>
			<Codec>eq</Codec>
			<Samplerate>lte</Samplerate>
			<Channel>eq</Channel>			
		</BypassIfMatch>
	</Audio>		
	<Audio>
		<Name>cond_audio_opus</Name>
		<Codec>opus</Codec>
		<Bitrate>128000</Bitrate>
		<Samplerate>48000</Samplerate>
		<Channel>2</Channel>
		<BypassIfMatch>
			<Codec>eq</Codec>
			<Samplerate>lte</Samplerate>
			<Channel>eq</Channel>	
		</BypassIfMatch>	
	</Audio>
</Encodes>
```



If a video track with a lower quality than the encoding option is input, unnecessary upscaling can be prevented.   **SAR (Storage Aspect Ratio)** is the ratio of original pixels. In the example below, even if the width and height of the original video are smaller than or equal to the width and height set in the encoding option, if the ratio is different, it means that encoding is performed without bypassing.

```xml
<Encodes>
	<Video>                                                                                                    
		<Name>prevent_upscaling_video</Name>
		<Codec>h264</Codec>
		<Bitrate>2048000</Bitrate>
		<Width>1280</Width>
		<Height>720</Height>
		<Framerate>30</Framerate>
		<BypassIfMatch>
			<Codec>eq</Codec>
			<Width>lte</Width>
			<Height>lte</Height>
			<SAR>eq</SAR>
		</BypassIfMatch>
	</Video>
</Encodes>
```

###

### **Keep the original with transcoding**

If you want to transcode with the same quality as the original. See the sample below for possible parameters that OME supports to keep original. If you remove the **Width**, **Height**, **Framerate**, **Samplerate**, and **Channel** parameters. then, It is transcoded with the same options as the original.

```xml
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

```xml
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

## Adaptive Bitrate Streaming (ABR)

From version 0.14.0, OvenMediaEngine can encode same source with multiple bitrates renditions and deliver it to the player.

As shown in the example configuration below, you can provide ABR by adding `<Playlists>` to `<OutputProfile>`. There can be multiple playlists, and each playlist can be accessed with `<FileName>`.

The method to access the playlist set through LLHLS is as follows.

`http[s]://<domain>[:port]/<app>/<stream>/`**`<FileName>`**`.m3u8`

The method to access the playlist set through HLS is as follows.

`http[s]://<domain>[:port]/<app>/<stream>/`**`<FileName>`**`.m3u8?format=ts`

The method to access the Playlist set through WebRTC is as follows.

`ws[s]://<domain>[:port]/<app>/<stream>/`**`<FileName>`**

Note that `<FileName>` must never contain the **`playlist`** and **`chunklist`** keywords. This is a reserved word used inside the system.

To set up `<Rendition>`, you need to add `<Name>` to the elements of `<Encodes>`. Connect the set `<Name>` into `<Rendition><Video>` or `<Rendition><Audio>`.

In the example below, three quality renditions are provided and the URL to play the `abr` playlist as LLHLS is `https://domain:port/app/stream/abr.m3u8` and The WebRTC playback URL is `wss://domain:port/app/stream/abr`

{% hint style="info" %}
TS files used in HLS must have A/V pre-muxed, so the `EnableTsPackaging` option must be set in the Playlist.
{% endhint %}

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
			<EnableTsPackaging>true</EnableTsPackaging>
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

| Protocol | Supported Codec   |
| -------- | ----------------- |
| WebRTC   | VP8, H.264, Opus  |
| LLHLS    | H.264, H.265, AAC |

Therefore, you set it up as shown in the table. If you want to stream using LLHLS, you need to set up H.264, H.265 and AAC, and if you want to stream using WebRTC, you need to set up Opus.

Also, if you are going to use WebRTC on all platforms, you need to configure both VP8 and H.264. This is because different codecs are supported for each browser, for example, VP8 only, H264 only, or both.

However, don't worry. If you set the codecs correctly, OME automatically sends the stream of codecs requested by the browser.
