# SRT (Beta)

Secure Reliable Transport (or SRT in short) is an open source video transport protocol and technology stack that optimizes streaming performance across unpredictable networks with secure streams and easy firewall traversal, bringing the best quality live video over the worst networks. We consider SRT to be one of the great alternatives to RTMP, and OvenMediaEngine can receive video streaming over SRT. For more information on SRT, please visit the [SRT Alliance website](https://www.srtalliance.org).

SRT uses the MPEG-TS format when transmitting live streams. This means that unlike RTMP, it can support many codecs. Currently, OvenMediaEngine supports H.264, H.265, and AAC codecs received by SRT.

## Configuration

### Bind

Set the SRT listen port as follows:

```markup
<Bind>
	<Providers>
		...
		<SRT>
			<Port>9999</Port>
			<!-- <WorkerCount>1</WorkerCount> -->
		</SRT>
	</Providers>
```

### Application

SRT input can be turned on/off for each application. As follows Setting  enables the SRT input function of the application.

```markup
<Applications>
	<Application>
		<Name>app</Name>
		<Providers>
			<SRT/>
```

## Encoders and streamid

There are various encoders that support SRT such as FFMPEG, OBS Studio, and srt-live-transmit. Please check the specifications of each encoder on how to transmit streams through SRT from the encoder. We describe an example using OBS Studio.

OvenMediaEngine classifies each stream using SRT's streamid. This means that unlike MEPG-TS/udp, OvenMediaEngine can receive multiple SRT streams through one port. For more information on streamid, see [Haivision's official documentation](https://github.com/Haivision/srt/blob/master/docs/features/access-control.md).

Therefore, in order for the SRT encoder to transmit a stream to OvenMediaEngine, the following information must be included in the streamid as [percent encoded](https://tools.ietf.org/html/rfc3986#section-2.1).

> streamid = percent\_encoding("srt://{host}\[:port]/{app name}/{stream name}\[?query=value]")

{% hint style="warning" %}
The **streamid** contains the URL format, so it must be [**percent encoded**](https://tools.ietf.org/html/rfc3986#section-2.1)****
{% endhint %}

### OBS Studio

OBS Studio 25.0 or later supports SRT. Please refer to the [OBS official documentation](https://obsproject.com/wiki/Streaming-With-SRT-Protocol) for more information. Enter the address of OvenMediaEngine in OBS Studio's Server as follows: When using SRT in OBS, you can leave the Stream Key blank.

`srt://ip:port?streamid=srt%3A%2F%2F{domain or IP address}[%3APort]%2F{App name}%2F{Stream name}`

![](<../.gitbook/assets/image (38).png>)
