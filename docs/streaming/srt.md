# SRT

OvenMediaEngine supports playback of streams delivered via RTMP, WebRTC, SRT, MPEG-2 TS, and RTSP using SRT-compatible players or integration with other SRT-enabled systems.

Currently, OvenMediaEngine supports H.264, H.265, and AAC codecs for SRT playback, ensuring the same compatibility as its [SRT provider functionality](../live-source/srt.md).

## Configuration

### Bind

To configure the port for SRT to listen on, use the following settings:

```xml
<Server>
    <Bind>
        <Publishers>
            <SRT>
                <Port>9998</Port>
                <!-- <WorkerCount>1</WorkerCount> -->
                <!--
                    To configure SRT socket options, you can use the settings shown below.
                    For more information, please refer to the details at the bottom of this document:
                    <Options>
                        <Option>...</Option>
                    </Options>
                -->
            </SRT>
...
```

### Application

You can control whether to enable SRT playback for each application. To activate this feature, configure the settings as shown below:

```xml
<Server>
    <VirtualHosts>
        <VirtualHost>
            <Applications>
                <Application>
                    <Name>app</Name>
                    <Providers>
                        <SRT />
...
```

## SRT client and `streamid`

As with using [SRT as a live source](../live-source/srt.md#encoders-and-streamid), multiple streams can be serviced on a single port. To distinguish each stream, you must set the `streamid` option to a value in the format `<virtual host>/<app>/<stream>`.

SRT clients such as FFmpeg, OBS Studio, and `srt-live-transmit` allow you to specify the `streamid` as a query string appended to the SRT URL. For example, you can use a percent-encoded string as the value of `streamid` like this: `srt://host:port?streamid=default%2Fapp%2Fstream`.

> streamid = percent\_encoding("{virtual host name}/{app name}/{stream name}")

## SRT Socket Options

You can configure SRT's socket options of the OvenMediaEngine server using `<Options>`. This is particularly useful when setting the encryption for SRT, and you can specify a passphrase by configuring as follows:

```xml
<Server>
    <Bind>
        <Publishers>
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
