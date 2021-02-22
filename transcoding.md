# Transcoding

OvenMediaEngine has a built-in live transcoder. The live transcoder can decode the incoming live source and re-encode it with the set codec or adjust the quality to encode at multiple bitrates.

## Supported Video and Audio Codecs

OvenMediaEngine currently supports the following codecs:

| Video Decoding | Audio Decoding | Video Encoding | Audio Encoding |
| :--- | :--- | :--- | :--- |
| H.264 \(Baseline\) | AAC | H.264 \(Baseline\) | AAC |
|  |  | VP8 | Opus |

## OutputProfiles

### OutputProfile

The `<OutputProfile>` setting allows incoming streams to be re-encoded via the  `<Encodes>` setting to create a new output stream. The name of the new output stream is determined by the rules set in `<OutputStreamName>`, and the newly created stream can be used according to the streaming URL format.

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

* **`WebRTC`**    ws://192.168.0.1:3333/app/`stream_bypass`
* **`HLS`**       http://192.168.0.1:8080/app/`stream_bypass`/playlist.m3u8
* **`MPEG-DASH`** http://192.168.0.1:8080/app/`stream_bypass`/manifest.mpd
* **`Low-Latency MPEG-DASH`** http://192.168.0.1:8080/app/`stream_bypass`/manifest\_ll.mpd

### Encodes

#### Video

You can set the video profile as below:

```markup
<Encodes>
    <Video>
        <Codec>vp8</Codec>
        <Width>1280</Width>
        <Height>720</Height>
        <Bitrate>2000000</Bitrate>
        <Framerate>30.0</Framerate>
    </Video>
</Encodes>
```

The meaning of each property is as follows:

| Property | Description |
| :--- | :--- |
| Codec | Specifies the `vp8` or `h264` codec to use |
| Width | Width of resolution |
| Height | Height of resolution |
| Bitrate | Bit per second |
| Framerate | Frames per second |

#### Audio

You can set the audio profile as below:

```markup
<Encodes>
    <Audio>
        <Codec>opus</Codec>
        <Bitrate>128000</Bitrate>
        <Samplerate>48000</Samplerate>
        <Channel>2</Channel>
    </Audio>
</Encodes>
```

The meaning of each property is as follows:

| Property | Description |
| :--- | :--- |
| Codec | Specifies the `opus` or `aac` codec to use |
| Bitrates | Bits per second |
| Samplerate | Samples per second |
| Channel | The number of audio channels |

#### Bypass transcoding

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

## Supported codecs by streaming protocol 

Even if you set up multiple codecs, there is a codec that matches each streaming protocol supported by OME, so it can automatically select and stream codecs that match the protocol. However, if you don't set a codec that matches the streaming protocol you want to use, it won't be streamed.

The following is a list of codecs that match each streaming protocol:

| Protocol | Supported Codec |
| :--- | :--- |
| WebRTC | VP8, H.264, Opus |
| HLS | H.264, AAC |
| Low-Latency MPEG-Dash | H.264, AAC |

Therefore, you set it up as shown in the table. If you want to stream using HLS or MPEG-DASH, you need to set up H.264 and AAC, and if you want to stream using WebRTC, you need to set up Opus.

Also, if you are going to use WebRTC on all platforms, you need to configure both VP8 and H.264. This is because different codecs are supported for each browser, for example, VP8 only, H264 only, or both.

However, don't worry. If you set the codecs correctly, OME automatically sends the stream of codecs requested by the browser.

{% hint style="info" %}
Currently, OME doesn't support adaptive streaming on HLS, MPEG-DASH. However, it will be updated soon.
{% endhint %}



