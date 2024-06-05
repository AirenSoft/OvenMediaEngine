# Low-Latency HLS

Apple supports Low-Latency HLS (LLHLS), which enables low-latency video streaming while maintaining scalability. LLHLS enables broadcasting with an end-to-end latency of about 2 to 5 seconds. OvenMediaEngine officially supports LLHLS as of v0.14.0.

LLHLS is an extension of HLS, so legacy HLS players can play LLHLS streams. However, the legacy HLS player plays the stream without using the low-latency function.

| Title     | Descriptions                    |
| --------- | ------------------------------- |
| Delivery  | <p>HTTP/1.1<br>HTTP/2</p>       |
| Security  | TLS (HTTPS)                     |
| Container | fMP4                            |
| Codecs    | <p>H.264</p><p>H.265<br>AAC</p> |

## Configuration

To use LLHLS, you need to add the `<LLHLS>` elements to the `<Publishers>` in the configuration as shown in the following example.

```markup
<Server version="8">
    <Bind>
        <Publishers>
            <LLHLS>
            <!-- 
        OME only supports h2, so LLHLS works over HTTP/1.1 on non-TLS ports. 
        LLHLS works with higher performance over HTTP/2, 
        so it is recommended to use a TLS port.
        -->
        <Port>80</Port>
        <TLSPort>443</TLSPort>
        <WorkerCount>1</WorkerCount>
        </LLHLS>
        </Publishers>
    </Bind>
    <VirtualHosts>
    <VirtualHost>
            <Applications>
                <Application>
                    <Publishers>
            <LLHLS>
                <ChunkDuration>0.2</ChunkDuration>
                <SegmentDuration>6</SegmentDuration>
                <SegmentCount>10</SegmentCount>
                <CrossDomains>
                    <Url>*</Url>
                </CrossDomains>
            </LLHLS>
                    </Publishers>
                </Application>
            </Applications>
        </VirtualHost>
    </VirtualHosts>
</Server>
```

| Element         | Decscription                                                                                                                                                                                                                               |
| --------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| Bind            | Set the HTTP ports to provide LLHLS.                                                                                                                                                                                                       |
| ChunkDuration   | Set the partial segment length to fractional seconds. This value affects low-latency HLS player. We recommend **0.2** seconds for this value.                                                                                              |
| SegmentDuration | Set the length of the segment in seconds. Therefore, a shorter value allows the stream to start faster. However, a value that is too short will make legacy HLS players unstable. Apple recommends **6** seconds for this value.           |
| SegmentCount    | The number of segments listed in the playlist. This value has little effect on LLHLS players, so use **10** as recommended by Apple. 5 is recommended for legacy HLS players. Do not set below 3. It can only be used for experimentation. |
| CrossDomains    | Control the domain in which the player works through `<CorssDomain>`. For more information, please refer to the [CrossDomain](broken-reference) section.                                                                                   |

{% hint style="info" %}
HTTP/2 outperforms HTTP/1.1, especially with LLHLS. Since all current browsers only support h2, HTTP/2 is supported only on TLS port. Therefore, it is highly recommended to use LLHLS on the TLS port.
{% endhint %}

## Adaptive Bitrates Streaming (ABR)

LLHLS can deliver adaptive bitrate streaming. OME encodes the same source with multiple renditions and delivers it to the players. And LLHLS Player, including OvenPlayer, selects the best quality rendition according to its network environment. Of course, these players also provide option for users to manually select rendition.

See the [Adaptive Bitrates Streaming](../transcoding/#adaptive-bitrates-streaming-abr) section for how to configure renditions.

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

## Streaming

LLHLS is ready when a live source is inputted and a stream is created. Viewers can stream using OvenPlayer or other players.

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

When you create a stream, as shown above, you can play LLHLS with the following URL:

> http\[s]://domain\[:port]/\<app name>/\<stream name>/llhls.m3u8

If you use the default configuration, you can start streaming with the following URL:

`https://domain:3334/app/<stream name>/llhls.m3u8`

We have prepared a test player that you can quickly see if OvenMediaEngine is working. Please refer to the [Test Player](../quick-start/test-player.md) for more information.

## Live Rewind

You can create as long a playlist as you want by setting `<DVR>` to the LLHLS publisher as shown below. This allows the player to rewind the live stream and play older segments. OvenMediaEngine stores and uses old segments in a file in `<DVR><TempStoragePath>` to prevent excessive memory usage. It stores as much as `<DVR><MaxDuration>` and the unit is seconds.

```xml
<LLHLS>
    ...
    <DVR>
        <Enable>true</Enable>
        <TempStoragePath>/tmp/ome_dvr/</TempStoragePath>
        <MaxDuration>3600</MaxDuration>
    </DVR>
    ...
</LLHLS>
```

## ID3v2 Timed Metadata

ID3 Timed metadata can be sent to the LLHLS stream through the [Send Event API](../rest-api/v1/virtualhost/application/stream/send-event.md).

## Dump

You can dump the LLHLS stream for VoD. You can enable it by setting the following in `<Application><Publishers><LLHLS>`. Dump function can also be controlled by [Dump API](../rest-api/v1/virtualhost/application/stream/hls-dump.md).

{% code overflow="wrap" %}
```xml
<LLHLS>
    <Dumps>
        <Dump>
            <Enable>true</Enable>
            <TargetStreamName>stream*</TargetStreamName>
            
            <Playlists>
                <Playlist>llhls.m3u8</Playlist>
                <Playlist>abr.m3u8</Playlist>
            </Playlists>
    
            <OutputPath>/service/www/ome-dev.airensoft.com/html/${VHostName}_${AppName}_${StreamName}/${YYYY}_${MM}_${DD}_${hh}_${mm}_${ss}</OutputPath>
        </Dump>
    </Dumps>
    ...
</LLHLS>
```
{% endcode %}

**TargetStreamName**

The name of the stream to dump to. You can use \* and ? to filter stream names.

**Playlists**

The name of the master playlist file to be dumped together.

**OutputPath**

The folder to output to. In the OutputPath you can use the macros shown in the table below. You must have write permission on the specified folder.

| Macro         | Description                    |
| ------------- | ------------------------------ |
| ${VHostName}  | Virtual Host Name              |
| ${AppName}    | Application Name               |
| ${StreamName} | Stream Name                    |
| ${YYYY}       | Year                           |
| ${MM}         | Month                          |
| ${DD}         | Day                            |
| ${hh}         | Hour                           |
| ${mm}         | Minute                         |
| ${ss}         | Second                         |
| ${S}          | Timezone                       |
| ${z}          | UTC offset (ex: +0900)         |
| ${ISO8601}    | Current time in ISO8601 format |

## DRM (beta)

OvenMediaEngine supports Widevine and Fairplay in LLHLS with simple setup since version 0.16.0.

{% hint style="warning" %}
Currently, DRM is only supported for H.264 and AAC codecs. Support for H.265 will be added soon.
{% endhint %}

To include DRM information in your LLHLS Publisher configuration, follow these steps. You can set the InfoFile path as either a relative path, starting from the directory where Server.xml is located, or as an absolute path.

```
<LLHLS>
    <ChunkDuration>0.5</ChunkDuration>
    <PartHoldBack>1.5</PartHoldBack>
    <SegmentDuration>6</SegmentDuration>
    <SegmentCount>10</SegmentCount>
    <DRM>
        <Enable>false</Enable>
        <InfoFile>path/to/file.xml</InfoFile>
    </DRM>
    <CrossDomains>
        <Url>*</Url>
    </CrossDomains>
</LLHLS>
```

The separation of the DRMInfoFile is designed to allow dynamic changes to the file. Any modifications to the DRMInfoFile will take effect when a new stream is generated.

Here's how you should structure your DRM Info File:

```
<?xml version="1.0" encoding="UTF-8"?>

<DRMInfo>
    <DRM>
        <Name>MultiDRM</Name>
        <VirtualHostName>default</VirtualHostName>
        <ApplicationName>app</ApplicationName>
        <StreamName>stream*</StreamName> <!-- Can be a wildcard regular expression -->
        <CencProtectScheme>cbcs</CencProtectScheme> <!-- Currently supports cbcs only -->
        <KeyId>572543f964e34dc68ba9ba9ef91d4xxx</KeyId> <!-- Hexadecimal -->
        <Key>16cf4232a86364b519e1982a27d90xxx</Key> <!-- Hexadecimal -->
        <Iv>572547f914e34dc68ba9ba9ef91d4xxx</Iv> <!-- Hexadecimal -->
        <Pssh>0000003f7073736800000000edef8ba979d64acea3c827dcd51d21ed0000001f1210572547f964e34dc68ba9ba9ef91d4c4a1a05657a64726d48f3c6899xxx</Pssh> <!-- Hexadecimal, for Widevine -->
        <!-- Add Pssh for FairPlay if needed -->
        <FairPlayKeyUrl>skd://fiarplay_key_url</FairPlayKeyUrl> <!-- FairPlay only -->
    </DRM>
    <DRM>
        <Name>MultiDRM2</Name>
        <VirtualHostName>default</VirtualHostName>
        <ApplicationName>app2</ApplicationName>
        <StreamName>stream*</StreamName> <!-- Can be a wildcard regular expression -->
         ...........
    </DRM>
</DRMInfo>
```

Multiple `<DRM>` can be set. Specify the VirtualHost, Application, and StreamName where DRM should be applied. StreamName supports wildcard regular expressions.

Currently, CencProtectScheme only supports "cbcs" since FairPlay also supports only cbcs. There may be limited prospects for adding other schemes in the near future.

KeyId, Key, Iv and Pssh values are essential and should be provided by your DRM provider. FairPlayKeyUrl is only need for FairPlay and if you want to enable FairPlay to your stream, it is required. It will be also provided by your DRM provider.

OvenPlayer now includes DRM-related options. Enable DRM and input the License URL. Your content is now securely protected.

<figure><img src="../.gitbook/assets/image (30).png" alt=""><figcaption></figcaption></figure>

### Pallycon DRM

{% hint style="danger" %}
Pallycon is no longer supported by the Open Source project and is only supported in the [Enterprise](https://ovenmediaengine-enterprise.gitbook.io/docs/getting-started/getting-started-with-ubuntu) version. For more information, see this [article](https://github.com/AirenSoft/OvenMediaEngine/discussions/1634).
{% endhint %}

OvenMediaEngine integrates with [Pallycon](https://pallycon.com/), allowing you to more easily apply DRM to LLHLS streams.

To integrate Pallycon, configure the DRMInfo.xml file as follows.

```xml
<?xml version="1.0" encoding="UTF-8"?>

<DRMInfo>
    <DRM>
        <Name>Pallycon</Name>
        <VirtualHostName>default</VirtualHostName>
        <ApplicationName>app</ApplicationName>
        <StreamName>stream*</StreamName> <!-- Can be wildcard regular expression -->

        <DRMProvider>Pallycon</DRMProvider> <!-- Manual(default), Pallycon -->
        <DRMSystem>Widevine,Fairplay</DRMSystem> <!-- Widevine, Fairplay -->
        <CencProtectScheme>cbcs</CencProtectScheme> <!-- cbcs, cenc -->
        <ContentId>${VHostName}_${AppName}_${StreamName}</ContentId>
        <KMSUrl>https://kms.pallycon.com/v2/cpix/pallycon/getKey/</KMSUrl>
        <KMSToken>xxxx</KMSToken>
    </DRM>
</DRMInfo>
```

Set `DRMProvider`to Pallycon. Then, set the necessary information as shown in the example. `KMSUrl` and `KMSToken` are values provided by the Pallycon console. `ContentId` can be created using VHostName, AppName, and StreamName macros.
