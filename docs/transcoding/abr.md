# ABR

## Adaptive Bitrate Streaming (ABR)

From version `0.14.0`, OvenMediaEngine can encode the same source with multiple bitrate renditions and deliver them to the player. You can provide ABR by adding `<Playlists>` to `<OutputProfile>`. There can be multiple playlists, and each playlist can be accessed with `<FileName>`.

{% hint style="info" %}
Note that `<FileName>` must never contain the **`playlist`** and **`chunklist`** keywords. This is a reserved word used inside the system.
{% endhint %}

Building on this, starting from version `0.18.0`, OvenMediaEngine provides a default playlist named `master` to simplify playback access. When simulcast input or ABR output is provided, the `master` dynamically includes the appropriate renditions and reflects the current encoding settings. This allows users to stream flexibly without additional configuration.

The method to access the playlist set through LLHLS is as follows:

* `http[s]://<host>[:port]/<app>/<stream>/master.m3u8`

The method to access the playlist set through HLS is as follows:

* `http[s]://<host>[:port]/<app>/<stream>/ts:master.m3u8`
* `http[s]://<host>[:port]/<app>/<stream>/master.m3u8?format=ts`

The method to access the Playlist set through WebRTC is as follows:

* `ws[s]://<host>[:port]/<app>/<stream>/master`

The method to access the Playlist set through SRT is as follows:

* `srt://<host>[:port]?streamid=<vhost>/<app>/<stream>/master`

#### For example

```xml
<OutputProfile>
	<Name>bypass_stream</Name>
	<OutputStreamName>${OriginStreamName}</OutputStreamName>

	<Playlist>
		<Name>default</Name>
		<FileName>master</FileName>
		<Options>
			<WebRtcAutoAbr>true</WebRtcAutoAbr>
			<HLSChunklistPathDepth>0</HLSChunklistPathDepth>
			<EnableTsPackaging>true</EnableTsPackaging>
		</Options>
		<RenditionTemplate>
			<Name>${Height}p</Name>
			<VideoTemplate>
				<EncodingType>all</EncodingType>
			</VideoTemplate>
			<AudioTemplate>
				<EncodingType>all</EncodingType>
			</AudioTemplate>
		</RenditionTemplate>
	</Playlist>

	<Encodes>
		<Video>
			<Name>bypass_video</Name>
			<Bypass>true</Bypass>
		</Video>
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
		<Audio>
			<Name>opus_audio</Name>
			<Codec>opus</Codec>
			<Bitrate>128000</Bitrate>
			<Samplerate>48000</Samplerate>
			<Channel>2</Channel>
			<BypassIfMatch>
				<Codec>eq</Codec>
			</BypassIfMatch>
		</Audio>	
	</Encodes>
</OutputProfile>
```

{% hint style="info" %}
TS files used in HLS must have A/V pre-muxed, so the `EnableTsPackaging` option must be set in the Playlist.
{% endhint %}

## Supported codecs by streaming protocol

Even if you set up multiple codecs, there is a codec that matches each streaming protocol supported by OME, so it can automatically select and stream codecs that match the protocol. However, if you don't set a codec that matches the streaming protocol you want to use, it won't be streamed.

The following is a list of codecs that match each streaming protocol:

| Protocol | Supported Codec         |
| -------- | ----------------------- |
| WebRTC   | VP8, H.264, H.265, Opus |
| LLHLS    | H.264, H.265, AAC       |

Therefore, you set it up as shown in the table. If you want to stream using LLHLS, you need to set up H.264, H.265 and AAC, and if you want to stream using WebRTC, you need to set up Opus. Also, if you are going to use WebRTC on all platforms, you need to configure both VP8 and H.264. This is because different codecs are supported for each browser, for example, VP8 only, H264 only, or both.

However, don't worry. If you set the codecs correctly, OME automatically sends the stream of codecs requested by the browser.
