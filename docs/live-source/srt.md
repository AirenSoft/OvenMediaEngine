# SRT

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

SRT input can be turned on/off for each application. As follows Setting enables the SRT input function of the application.

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
The **streamid** contains the URL format, so it must be [**percent encoded**](https://tools.ietf.org/html/rfc3986#section-2.1)\*\*\*\*
{% endhint %}

### OBS Studio

OBS Studio 25.0 or later supports SRT. Please refer to the [OBS official documentation](https://obsproject.com/wiki/Streaming-With-SRT-Protocol) for more information. Enter the address of OvenMediaEngine in OBS Studio's Server as follows: When using SRT in OBS, leave the Stream Key blank.

`srt://{full domain or IP address}:port?streamid=srt%3A%2F%2F{full domain or IP address}[%3APort]%2F{App name}%2F{Stream name}&latency=2000000`

The `streamid` has to be the urlencoded address of the server name as specified in the ome server configuration plus the app name and the stream name, each separated by `/`. The `latency` configures the size of the server-side recive buffer and the time limit for SRT in nanoseconds. Typical value for latency are 150000 (150ms) for stremaing to a server in the local network, 600000 (600ms) for streaming to a server over the internet in the local region, and 2000000 (2 seconds) when stremaing over long distance.

![](<../.gitbook/assets/image (38).png>)

### Blackmagic Web Presenter

For configuring a Blackmagic Web Presenter or similar device to stream to OvenMediaEngine over SRT, export the streaming xml configuration and set the `<streaming>` section to something like this:

```markup
<streaming>
    <service>
        <name>SRT-Sample-Service</name>
        <servers>
            <server group="Primary">
                <name>Some-server-name</name>
                <url>srt://your-server-domain.com:9999</url>
                <srt-extensions>
                    <stream-id>
                        <item key="streamid" value="srt://your-server-domain.com:9999/appname/streamname123"/>
                    </stream-id>
                    <stream-latency>
                        <item key="latency" value="2000"/>
                    </stream-latency>
                </srt-extensions>
            </server>
        </servers>
        <profiles default="Streaming Sample">
            <profile>
                <name>Streaming Sample</name>
                <low-latency/>
                <config resolution="1080p" fps="60">
                    <bitrate>6000000</bitrate>
                    <audio-bitrate>128000</audio-bitrate>
                    <keyframe-interval>2</keyframe-interval>
                </config>
            </profile>
        </profiles>
    </service>
</streaming>
```

The Web Presenter requires the latency value to be provided in milliseconds and the streamid should be the not-encoded URL.

## SRT Socket Options

You can configure SRT's socket options of the OvenMediaEngine server using `<Options>`. This is particularly useful when setting the encryption for SRT, and you can specify a passphrase by configuring as follows:

```xml
<Server>
    <Bind>
        <Providers>
            <SRT>
                ...
                <Options>
                    <Option>
                        <Key>SRTO_PBKEYLEN</Key>
                        <Value>16</Value>
                    </Option>
                    <Option>
                        <Key>SRTO_PASSPHRASE</Key>
                        <Value>thisismypassphrase</Value>
                    </Option>
                </Options>
            </SRT>
...
```

For more information on SRT socket options, please refer to [https://github.com/Haivision/srt/blob/master/docs/API/API-socket-options.md#list-of-options](https://github.com/Haivision/srt/blob/master/docs/API/API-socket-options.md#list-of-options).
