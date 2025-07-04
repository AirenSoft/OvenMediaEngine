# SRT

Secure Reliable Transport (or SRT in short) is an open source video transport protocol and technology stack that optimizes streaming performance across unpredictable networks with secure streams and easy firewall traversal, bringing the best quality live video over the worst networks. We consider SRT to be one of the great alternatives to RTMP, and OvenMediaEngine can receive video streaming over SRT. For more information on SRT, please visit the [SRT Alliance website](https://www.srtalliance.org).

<table><thead><tr><th width="290">Title</th><th>Functions</th></tr></thead><tbody><tr><td>Container</td><td>MPEG-2 TS</td></tr><tr><td>Transport</td><td>SRT</td></tr><tr><td>Codec</td><td>H.264, H.265, AAC</td></tr><tr><td>Additional Features</td><td>Simulcast</td></tr></tbody></table>

SRT uses the MPEG-TS format when transmitting live streams. This means that unlike RTMP, it can support many codecs. Currently, OvenMediaEngine supports H.264, H.265 and AAC codecs received by SRT.

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

OvenMediaEngine classifies each stream using SRT's streamid. This means that unlike MPEG-TS/udp, OvenMediaEngine can receive multiple SRT streams through one port. For more information on streamid, see [Haivision's official documentation](https://github.com/Haivision/srt/blob/master/docs/features/access-control.md).

Therefore, in order for the SRT encoder to transmit a stream to OvenMediaEngine, the following information must be included in the streamid as [percent encoded](https://tools.ietf.org/html/rfc3986#section-2.1).

> streamid = {vhost name}/{app name}/{stream name}

### OBS Studio

OBS Studio 25.0 or later supports SRT. Please refer to the [OBS official documentation](https://obsproject.com/wiki/Streaming-With-SRT-Protocol) for more information. Enter the address of OvenMediaEngine in OBS Studio's Server as follows: When using SRT in OBS, leave the Stream Key blank.

`srt://{full domain or IP address}:port?streamid={vhost name}/{app name}/{stream name}`

<figure><img src="../.gitbook/assets/image (61).png" alt=""><figcaption></figcaption></figure>

### Blackmagic Web Presenter

For configuring a Blackmagic Web Presenter, ATEM Mini Pro or similar device to stream to OvenMediaEngine over SRT, choose the "Custom URL H264/H265" platform option with the following syntax:

```
Server: srt://your-server-domain.com:<SRT_PORT>
Key: <PERCENT_ENCODED_STREAM_ID>
```

The default streaming profiles work well, and there are more advanced configuration options available if you [edit the streaming.xml settings file](https://airensoft.gitbook.io/ovenmediaengine/v/0.16.4/live-source/srt-beta#blackmagic-web-presenter)

## Multiple Audio Track

The SRT Provider supports multiple audio track inputs. This is automatically applied to the LLHLS Publisher.

If you want to label the input audio tracks, configure them as follows. This affects the player's audio selection UI when playing LLHLS.

```xml
<Application>
  <Providers>
    <SRT>
      <AudioMap>
        <Item>
          <Name>English</Name> 
          <Language>en</Language> <!-- Optioanl, RFC 5646 -->
          <Characteristics>public.accessibility.describes-video</Characteristics> <!-- Optional -->
        </Item>
        <Item>
          <Name>Korean</Name>
          <Language>ko</Language> <!-- Optioanl, RFC 5646 -->
          <Characteristics>public.alternate</Characteristics> <!-- Optional -->
        </Item>
        <Item>
          <Name>Japanese</Name>
          <Language>ja</Language> <!-- Optioanl, RFC 5646 -->
          <Characteristics>public.alternate</Characteristics> <!-- Optional -->
        </Item>
      </AudioMap>
   ...
```

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
