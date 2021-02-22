# Classes

## Response&lt;`[TYPE]`&gt; <a id="responsetype"></a>

| Type | Name | Optional | Description | Examples |
| :--- | :--- | :--- | :--- | :--- |
| Int | statusCode | N | Status code | `200` |
| String | message | N | A message describing the value returned | `"OK"` |
| `[TYPE]` | response | Y | A response data | `{}` |

## VirtualHost <a id="virtualhost"></a>

| Type | Name | Optional | Description | Examples |
| :--- | :--- | :--- | :--- | :--- |
| String | name | N | A name of Virtual Host | `"default"` |
| `Host` | td | Y | Host | `Host` |
| `SignedPolicy` | signedPolicy | Y | SignedPolicy | `SignedPolicy` |
| `SignedToken` | signedToken | Y | SignedToken | `SignedToken` |
| List&lt;`OriginMap`&gt; | td | Y | A list of Origin map | `[OriginMap, OriginMap, ...]` |

## Host <a id="host"></a>

| Type | Name | Optional | Description | Examples |
| :--- | :--- | :--- | :--- | :--- |
| List&lt;String&gt; | td | N | A list of hosts | `["airensoft.com", "*.test.com", ...]` |
| Tls | td | Y | TLS | `TLS` |

## TLS <a id="tls"></a>

| Type | Name | Optional | Description | Examples |
| :--- | :--- | :--- | :--- | :--- |
| String | certPath | N | A path of cert file | `"a.crt"` |
| String | keyPath | N | A path of private key file | `"a.key"` |
| String | chainCertPath | Y | A path of chain cert file | `"c.crt"` |

## SignedPolicy <a id="signedpolicy"></a>

| Type | Name | Optional | Description | Examples |
| :--- | :--- | :--- | :--- | :--- |
| String | policyQueryKey | N |  |  |
| String | signatureQueryKey | N |  |  |
| String | secretKey | N |  |  |

## SignedToken <a id="signedtoken"></a>

| Type | Name | Optional | Description | Examples |
| :--- | :--- | :--- | :--- | :--- |
| String | cryptoKey | N |  |  |
| String | queryStringKey | N |  |  |

## OriginMap <a id="originmap"></a>

| Type | Name | Optional | Description | Examples |
| :--- | :--- | :--- | :--- | :--- |
| String | location | N | A pattern to map origin | `"/"` |
| `Pass` | pass | N | What to request with Origin if the pattern matches | `Pass` |

## Pass <a id="pass"></a>

| Type | Name | Optional | Description | Examples |
| :--- | :--- | :--- | :--- | :--- |
| String | scheme | N | Scheme to distinguish the provider | `"ovt"` |
| List&lt;String&gt; | urls | N | An address list to pull from provider | `["origin:9000", "origin2:9000", ...]` |

## Application <a id="application"></a>

| Type | Name | Optional | Description | Examples |
| :--- | :--- | :--- | :--- | :--- |
| String | name | N | App name \(You cannot change this value after you create it\) | `"app"` |
| Bool | dynamic | N | Whether the app was created using `PullStream()` | `true` |
| Enum&lt;`ApplicationType`&gt; | type | N | App type | `"live"` |
| `Providers` | providers | Y | A list of `Provider`s | `Providers` |
| `Publishers` | publishers | Y | A list of `Publisher`s | `Publishers` |
| List&lt;`OutputProfile`&gt; | outputProfiles | Y | A list of `OutputProfile`s | `[OutputProfile, OutputProfile, ...]` |

## Providers <a id="providers"></a>

| Type | Name | Optional | Description | Examples |
| :--- | :--- | :--- | :--- | :--- |
| `RtmpProvider` | rtmp | Y |  | `RtmpProvider` |
| `RtspPullProvider` | rtspPull | Y |  | `RtspPullProvider` |
| `RtspProvider` | rtsp | Y |  | `RtspProvider` |
| `OvtProvider` | ovt | Y |  | `OvtProvider` |
| `MpegtsProvider` | mpegts | Y |  | `MpegtsProvider` |

## RtmpProvider <a id="rtmpprovider"></a>

| Type | Name | Optional | Description | Examples |
| :--- | :--- | :--- | :--- | :--- |
| \(Reserved for future use\) | - | - | - |  |

## RtspPullProvider <a id="rtsppullprovider"></a>

| Type | Name | Optional | Description | Examples |
| :--- | :--- | :--- | :--- | :--- |
| \(Reserved for future use\) | - | - | - |  |

## RtspProvider <a id="rtspprovider"></a>

| Type | Name | Optional | Description | Examples |
| :--- | :--- | :--- | :--- | :--- |
| \(Reserved for future use\) | - | - | - |  |

## OvtProvider <a id="ovtprovider"></a>

| Type | Name | Optional | Description | Examples |
| :--- | :--- | :--- | :--- | :--- |
| \(Reserved for future use\) | - | - | - |  |

## MpegtsProvider <a id="mpegtsprovider"></a>

| Type | Name | Optional | Description | Examples |
| :--- | :--- | :--- | :--- | :--- |
| List&lt;`MpegtsStream`&gt; | streams | Y | MPEG-TS Stream map | `[MpegtsStream, MpegtsStream, ...]` |

## MpegtsStream <a id="mpegtsstream"></a>

| Type | Name | Optional | Description | Examples |
| :--- | :--- | :--- | :--- | :--- |
| String | name | N | A name to generate when MPEG-TS stream is received | `"stream"` |
| `RangedPort` | port | Y | MPEG-TS Port | `"40000-40001/udp"` |

## Publishers <a id="publishers"></a>

| Type | Name | Optional | Description | Examples |
| :--- | :--- | :--- | :--- | :--- |
| Int | threadCount | N | Number of threads | `4` |
| `RtmpPushPublisher` | rtmpPush | Y |  | `RtmpPushPublisher` |
| `HlsPublisher` | hls | Y |  | `HlsPublisher` |
| `DashPublisher` | dash | Y |  | `DashPublisher` |
| `LlDashPublisher` | llDash | Y |  | `LlDashPublisher` |
| `WebrtcPublisher` | webrtc | Y |  | `WebrtcPublisher` |
| `OvtPublisher` | ovt | Y |  | `OvtPublisher` |
| `FilePublisher` | file | Y |  | `FilePublisher` |
| `ThumbnailPublisher` | thumbnail | Y |  | `ThumbnailPublisher` |

## RtmpPushPublisher <a id="rtmppushpublisher"></a>

| Type | Name | Optional | Description | Examples |
| :--- | :--- | :--- | :--- | :--- |
| \(Reserved for future use\) | - | - | - |  |

## HlsPublisher <a id="hlspublisher"></a>

| Type | Name | Optional | Description | Examples |
| :--- | :--- | :--- | :--- | :--- |
| Int | segmentCount | N | Segment count in the playlist.m3u8 | `3` |
| Int | segmentDuration | N | Segment duration \(unit: seconds\) | `4` |
| List&lt;String&gt; | crossDomains | Y | Cross domain URLs | `["*"]` |

## DashPublisher <a id="dashpublisher"></a>

| Type | Name | Optional | Description | Examples |
| :--- | :--- | :--- | :--- | :--- |
| Int | segmentCount | N | Segment count in the manifest.mpd | `3` |
| Int | segmentDuration | N | Segment duration \(unit: seconds\) | `4` |
| List&lt;String&gt; | crossDomains | Y | Cross domain URLs | `["*"]` |

## LlDashPublisher <a id="lldashpublisher"></a>

| Type | Name | Optional | Description | Examples |
| :--- | :--- | :--- | :--- | :--- |
| Int | segmentDuration | N | Segment duration \(unit: seconds\) | `3` |
| List&lt;String&gt; | crossDomains | Y | Cross domain URLs | `["*"]` |

## WebrtcPublisher <a id="webrtcpublisher"></a>

| Type | Name | Optional | Description | Examples |
| :--- | :--- | :--- | :--- | :--- |
| `TimeInterval` | timeout | Y | ICE timeout \(unit: seconds\) | `30` |

## OvtPublisher <a id="ovtpublisher"></a>

| Type | Name | Optional | Description | Examples |
| :--- | :--- | :--- | :--- | :--- |
| \(Reserved for future use\) | - | - | - |  |

## FilePublisher <a id="filepublisher"></a>

| Type | Name | Optional | Description | Examples |
| :--- | :--- | :--- | :--- | :--- |
| String | filePath | Y | A path to store recorded file  You can use the following macros: `${TransactionId}: An identifier of transaction ${Id}: An identifier to distinguish files ${StartTime:YYYYMMDDhhmmss}: Start time of recording ${EndTime:YYYYMMDDhhmmss}: End time of of recording ${VirtualHost}: A name of virtual host ${Application}: A name of application ${SourceStream}: A name of input stream ${Stream}: A name of output stream ${Sequence}: A sequence number`  | `"/tmp/${StartTime:YYYYMMDDhhmmss}_${Stream}.mp4"` |
| String | fileInfoPath | Y | A path of recorded files | `"/tmp/${StartTime:YYYYMMDDhhmmss}_${Stream}.xml"` |

## ThumbnailPublisher <a id="thumbnailpublisher"></a>

| Type | Name | Optional | Description | Examples |
| :--- | :--- | :--- | :--- | :--- |
| List&lt;String&gt; | crossDomains | Y | Cross domain URLs | `["*"]` |

## OutputProfile <a id="outputprofile"></a>

| Type | Name | Optional | Description | Examples |
| :--- | :--- | :--- | :--- | :--- |
| String | name | N | A name of `OutputProfile` | `"bypass_stream"` |
| String | outputStreamName | N | A name of output stream | `"${OriginStreamName}"` |
| `Encodes` | encodes | Y |  | `[Encodes, Encodes, ...]` |

## Encodes <a id="encodes"></a>

| Type | Name | Optional | Description | Examples |
| :--- | :--- | :--- | :--- | :--- |
| List&lt;`Video`&gt; | videos | Y |  | `[Video, Video, ...]` |
| List&lt;`Audio`&gt; | audios | Y |  | `[Audio, Audio, ...]` |
| List&lt;`Image`&gt; | images | Y |  | `[Image, Image, ...]` |

## Video <a id="video"></a>

| Type | Name | Optional | Description | Examples |
| :--- | :--- | :--- | :--- | :--- |
| Bool | bypass | Y |  | `true` |
| Enum&lt;`Codec`&gt; | codec | Conditional | Video codec | `"h264"` |
| Int | width | Conditional |  | `1280` |
| Int | height | Conditional |  | `720` |
| String | bitrate | Conditional | bitrate \(You can use "K" or "M" suffix like `100K`, `3M`\) | `"3000000" "2.5M"` |
| Float | framerate | Conditional |  | `29.997` |

## Audio <a id="audio"></a>

| Type | Name | Optional | Description | Examples |
| :--- | :--- | :--- | :--- | :--- |
| Bool | bypass | Y |  | `true` |
| Enum&lt;`Codec`&gt; | codec | Conditional | Audio codec | `"opus"` |
| Int | samplerate | Conditional |  | `48000` |
| Int | channel | Conditional |  | `2` |
| String | bitrate | Conditional | bitrate \(You can use "K" or "M" suffix like `128K`, `0.1M`\) | `"128000" "128K"` |

## Image <a id="image"></a>

| Type | Name | Optional | Description | Examples |
| :--- | :--- | :--- | :--- | :--- |
| Enum&lt;`Codec`&gt; | codec | N |  | `"jpeg" | "png"` |
| Int | width | Conditional |  | `854` |
| Int | height | Conditional |  | `480` |
| Float | framerate | N | An interval of image creation | `1` |

## Stream <a id="stream"></a>

| Type | Name | Optional | Description | Examples |
| :--- | :--- | :--- | :--- | :--- |
| String | name | N | A name of stream | `"stream"` |
| `InputStream` | input | N | An information of input stream | `InputStream` |
| List&lt;`OutputStream`&gt; | outputs | N | An information of output streams | `[OutputStream, OutputStream, ...]` |

## NewStream <a id="newstream"></a>

| Type | Name | Optional | Description | Examples |
| :--- | :--- | :--- | :--- | :--- |
| String | name | N | A name of stream to create | `"stream"` |
| `PullStream` | pull | Y | pull | `PullStream` |
| `MpegtsStream` | mpegts | Y | Creates a `prestream` | `MpegtsStream` |

## PullStream <a id="pullstream"></a>

| Type | Name | Optional | Description | Examples |
| :--- | :--- | :--- | :--- | :--- |
| String | url | N | URL to pull | `"rtsp://host.com/resource"` |

## InputStream <a id="inputstream"></a>

| Type | Name | Optional | Description | Examples |
| :--- | :--- | :--- | :--- | :--- |
| String | agent | Y | A name of broadcast tool | `"OBS 12.0.4"` |
| String | from | N | URI stream created | `"tcp://192.168.0.200:33399"` |
| String | to | Y | URI represents connection with the input | `"rtmp://dev.airensoft.com:1935"` |
| List&lt;`Track`&gt; | tracks | N | A list of tracks in input stream | `[Track, Track, ...]` |
| `Timestamp` | createdTime | N | Creation time | `"2020-10-30T11:00:00+09:00"` |

## OutputStream <a id="outputstream"></a>

| Type | Name | Optional | Description | Examples |
| :--- | :--- | :--- | :--- | :--- |
| String | name | N | An name of `OutputStream` | `"stream_o"` |
| List&lt;`Track`&gt; | tracks | N | A list of tracks in `OutputStream` | `[Track, Track, ...]` |

## Track <a id="track"></a>

| Type | Name | Optional | Description | Examples |
| :--- | :--- | :--- | :--- | :--- |
| Enum&lt;`MediaType`&gt; | type | Y | Media type | `"video"` |
| `Video` | video | Conditional | A configuration of video encoding | `Video` |
| `Audio` | audio | Conditional | A configuration of audio encoding | `Audio` |

## VideoTrack <a id="videotrack"></a>

| Type | Name | Optional | Description | Examples |
| :--- | :--- | :--- | :--- | :--- |
| \(Extends `Video`\) | - | - |  |  |
| `Timebase` | timebase | Y | Timebase | `Timebase` |

## Timebase <a id="timebase"></a>

| Type | Name | Optional | Description | Examples |
| :--- | :--- | :--- | :--- | :--- |
| Int | num | N | Numerator | `1` |
| Int | den | N | Denominator | `90000` |

## AudioTrack <a id="audiotrack"></a>

| Type | Name | Optional | Description | Examples |
| :--- | :--- | :--- | :--- | :--- |
| \(Extends `Audio`\) | - | - |  | `true` |
| `Timebase` | timebase | Y | Timebase | `Timebase` |

## Record <a id="record"></a>

| Type | Name | Optional | Description | Examples |
| :--- | :--- | :--- | :--- | :--- |
| String | id | Y | Unique identifier |  |
| `OutputStream` | streams | N | A combination of output stream's track name and track id |  |
| Enum&lt;`SessionState`&gt; | state | N | Record state |  |
| String | filePath | N | A path of recorded files |  |
| String | fileInfoPath | N | A path of recorded file informations |  |
| String | recordedBytes | N | Recorded bytes |  |
| Int | recordedTime | N | Recorded time |  |
| `Timestamp` | startTime | N | Started time |  |
| `Timestamp` | finishTime | N | Finished time |  |
| Int | bitrate | N | Average bitrate |  |

## Push <a id="push"></a>

| Type | Name | Optional | Description | Examples |
| :--- | :--- | :--- | :--- | :--- |
| String | id | N | Unique identifier |  |
| `OutputStream` | stream | Y | A combination of output stream's track name and track id |  |
| Enum&lt;`StreamSourceType`&gt; | protocol | Y | Protocol of input stream |  |
| String | url | Y | Destination URL |  |
| String | streamKey | Conditional | Stream key of destination |  |
| Enum&lt;`SessionState`&gt; | state | N | Push state |  |
| Int | sentBytes | N | Sent bytes |  |
| Int | sentPackets | N | Sent packets count |  |
| Int | sentErrorBytes | N | Error bytes |  |
| Int | sentErrorPackets | N | Error packets count  |  |
| Int | reconnect | N | Reconnect count |  |
| `Timestamp` | startTime | N | Started time |  |
| `Timestamp` | finishTime | N | Finished time |  |
| Int | bitrate | N | Average bitrate |  |

## CommonMetrics <a id="commonmetrics"></a>

| Type | Name | Optional | Description | Examples |
| :--- | :--- | :--- | :--- | :--- |
| `Timestamp` | createdTime | N | Creation time | `"2020-10-30T11:00:00+09:00"` |
| `Timestamp` | lastUpdatedTime | N | Modified time | `"2020-10-30T11:00:00+09:00"` |
| Long | totalBytesIn | N | Received bytes | `3109481213` |
| Long | totalBytesOut | N | Sent bytes | `1230874123` |
| Int | totalConnections | N | Current connections | `10` |
| Int | maxTotalConnections | N | Max connections since the stream is created | `293` |
| `Timestamp` | maxTotalConnectionTime | N | When the maximum number of concurrent connections has been updated. | `"2020-10-30T11:00:00+09:00"` |
| `Timestamp` | lastRecvTime | N | Last time data was received | `"2020-10-30T11:00:00+09:00"` |
| `Timestamp` | lastSentTime | N | Last time data was sent | `"2020-10-30T11:00:00+09:00"` |

## StreamMetrics <a id="streammetrics"></a>

| Type | Name | Optional | Description | Examples |
| :--- | :--- | :--- | :--- | :--- |
| \(Extends `CommonMetrics`\) | - | - | Includes all fields of `CommonMetrics` |  |
| `TimeInterval` | requestTimeToOrigin | Y | A elapsed time to connect to Origin | `1000` |
| `TimeInterval` | responseTimeFromOrigin | Y | A elapsed time from Origin to respond | `10000` |

