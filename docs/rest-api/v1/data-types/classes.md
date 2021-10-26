# Classes

## Response<`[TYPE]`> <a href="responsetype" id="responsetype"></a>

| Type     | Name       | Optional | Description                             | Examples |
| -------- | ---------- | -------- | --------------------------------------- | -------- |
| Int      | statusCode | N        | Status code                             | `200`    |
| String   | message    | N        | A message describing the value returned | `"OK"`   |
| `[TYPE]` | response   | Y        | A response data                         | `{}`     |

## VirtualHost <a href="virtualhost" id="virtualhost"></a>

| Type              | Name         | Optional | Description            | Examples                      |
| ----------------- | ------------ | -------- | ---------------------- | ----------------------------- |
| String            | name         | N        | A name of Virtual Host | `"default"`                   |
| `Host`            | td           | Y        | Host                   | `Host`                        |
| `SignedPolicy`    | signedPolicy | Y        | SignedPolicy           | `SignedPolicy`                |
| `SignedToken`     | signedToken  | Y        | SignedToken            | `SignedToken`                 |
| List<`OriginMap`> | td           | Y        | A list of Origin map   | `[OriginMap, OriginMap, ...]` |

## Host <a href="host" id="host"></a>

| Type          | Name | Optional | Description     | Examples                               |
| ------------- | ---- | -------- | --------------- | -------------------------------------- |
| List\<String> | td   | N        | A list of hosts | `["airensoft.com", "*.test.com", ...]` |
| Tls           | td   | Y        | TLS             | `TLS`                                  |

## TLS <a href="tls" id="tls"></a>

| Type   | Name          | Optional | Description                | Examples  |
| ------ | ------------- | -------- | -------------------------- | --------- |
| String | certPath      | N        | A path of cert file        | `"a.crt"` |
| String | keyPath       | N        | A path of private key file | `"a.key"` |
| String | chainCertPath | Y        | A path of chain cert file  | `"c.crt"` |

## SignedPolicy <a href="signedpolicy" id="signedpolicy"></a>

| Type   | Name              | Optional | Description | Examples |
| ------ | ----------------- | -------- | ----------- | -------- |
| String | policyQueryKey    | N        |             |          |
| String | signatureQueryKey | N        |             |          |
| String | secretKey         | N        |             |          |

## SignedToken <a href="signedtoken" id="signedtoken"></a>

| Type   | Name           | Optional | Description | Examples |
| ------ | -------------- | -------- | ----------- | -------- |
| String | cryptoKey      | N        |             |          |
| String | queryStringKey | N        |             |          |

## OriginMap <a href="originmap" id="originmap"></a>

| Type   | Name     | Optional | Description                                        | Examples |
| ------ | -------- | -------- | -------------------------------------------------- | -------- |
| String | location | N        | A pattern to map origin                            | `"/"`    |
| `Pass` | pass     | N        | What to request with Origin if the pattern matches | `Pass`   |

## Pass <a href="pass" id="pass"></a>

| Type          | Name   | Optional | Description                           | Examples                               |
| ------------- | ------ | -------- | ------------------------------------- | -------------------------------------- |
| String        | scheme | N        | Scheme to distinguish the provider    | `"ovt"`                                |
| List\<String> | urls   | N        | An address list to pull from provider | `["origin:9000", "origin2:9000", ...]` |

## Application <a href="application" id="application"></a>

| Type                    | Name           | Optional | Description                                                 | Examples                              |
| ----------------------- | -------------- | -------- | ----------------------------------------------------------- | ------------------------------------- |
| String                  | name           | N        | App name (You cannot change this value after you create it) | `"app"`                               |
| Bool                    | dynamic        | N        | Whether the app was created using `PullStream()`            | `true`                                |
| Enum<`ApplicationType`> | type           | N        | App type                                                    | `"live"`                              |
| `Providers`             | providers      | Y        | A list of `Provider`s                                       | `Providers`                           |
| `Publishers`            | publishers     | Y        | A list of `Publisher`s                                      | `Publishers`                          |
| List<`OutputProfile`>   | outputProfiles | Y        | A list of `OutputProfile`s                                  | `[OutputProfile, OutputProfile, ...]` |

## Providers <a href="providers" id="providers"></a>

| Type               | Name     | Optional | Description | Examples           |
| ------------------ | -------- | -------- | ----------- | ------------------ |
| `RtmpProvider`     | rtmp     | Y        |             | `RtmpProvider`     |
| `RtspPullProvider` | rtspPull | Y        |             | `RtspPullProvider` |
| `RtspProvider`     | rtsp     | Y        |             | `RtspProvider`     |
| `OvtProvider`      | ovt      | Y        |             | `OvtProvider`      |
| `MpegtsProvider`   | mpegts   | Y        |             | `MpegtsProvider`   |

## RtmpProvider <a href="rtmpprovider" id="rtmpprovider"></a>

| Type                      | Name | Optional | Description | Examples |
| ------------------------- | ---- | -------- | ----------- | -------- |
| (Reserved for future use) | -    | -        | -           |          |

## RtspPullProvider <a href="rtsppullprovider" id="rtsppullprovider"></a>

| Type                      | Name | Optional | Description | Examples |
| ------------------------- | ---- | -------- | ----------- | -------- |
| (Reserved for future use) | -    | -        | -           |          |

## RtspProvider <a href="rtspprovider" id="rtspprovider"></a>

| Type                      | Name | Optional | Description | Examples |
| ------------------------- | ---- | -------- | ----------- | -------- |
| (Reserved for future use) | -    | -        | -           |          |

## OvtProvider <a href="ovtprovider" id="ovtprovider"></a>

| Type                      | Name | Optional | Description | Examples |
| ------------------------- | ---- | -------- | ----------- | -------- |
| (Reserved for future use) | -    | -        | -           |          |

## MpegtsProvider <a href="mpegtsprovider" id="mpegtsprovider"></a>

| Type                 | Name    | Optional | Description        | Examples                            |
| -------------------- | ------- | -------- | ------------------ | ----------------------------------- |
| List<`MpegtsStream`> | streams | Y        | MPEG-TS Stream map | `[MpegtsStream, MpegtsStream, ...]` |

## MpegtsStream <a href="mpegtsstream" id="mpegtsstream"></a>

| Type         | Name | Optional | Description                                        | Examples            |
| ------------ | ---- | -------- | -------------------------------------------------- | ------------------- |
| String       | name | N        | A name to generate when MPEG-TS stream is received | `"stream"`          |
| `RangedPort` | port | Y        | MPEG-TS Port                                       | `"40000-40001/udp"` |

## Publishers <a href="publishers" id="publishers"></a>

| Type                 | Name        | Optional | Description       | Examples             |
| -------------------- | ----------- | -------- | ----------------- | -------------------- |
| Int                  | threadCount | N        | Number of threads | `4`                  |
| `RtmpPushPublisher`  | rtmpPush    | Y        |                   | `RtmpPushPublisher`  |
| `HlsPublisher`       | hls         | Y        |                   | `HlsPublisher`       |
| `DashPublisher`      | dash        | Y        |                   | `DashPublisher`      |
| `LlDashPublisher`    | llDash      | Y        |                   | `LlDashPublisher`    |
| `WebrtcPublisher`    | webrtc      | Y        |                   | `WebrtcPublisher`    |
| `OvtPublisher`       | ovt         | Y        |                   | `OvtPublisher`       |
| `FilePublisher`      | file        | Y        |                   | `FilePublisher`      |
| `ThumbnailPublisher` | thumbnail   | Y        |                   | `ThumbnailPublisher` |

## RtmpPushPublisher <a href="rtmppushpublisher" id="rtmppushpublisher"></a>

| Type                      | Name | Optional | Description | Examples |
| ------------------------- | ---- | -------- | ----------- | -------- |
| (Reserved for future use) | -    | -        | -           |          |

## HlsPublisher <a href="hlspublisher" id="hlspublisher"></a>

| Type          | Name            | Optional | Description                        | Examples |
| ------------- | --------------- | -------- | ---------------------------------- | -------- |
| Int           | segmentCount    | N        | Segment count in the playlist.m3u8 | `3`      |
| Int           | segmentDuration | N        | Segment duration (unit: seconds)   | `4`      |
| List\<String> | crossDomains    | Y        | Cross domain URLs                  | `["*"]`  |

## DashPublisher <a href="dashpublisher" id="dashpublisher"></a>

| Type          | Name            | Optional | Description                       | Examples |
| ------------- | --------------- | -------- | --------------------------------- | -------- |
| Int           | segmentCount    | N        | Segment count in the manifest.mpd | `3`      |
| Int           | segmentDuration | N        | Segment duration (unit: seconds)  | `4`      |
| List\<String> | crossDomains    | Y        | Cross domain URLs                 | `["*"]`  |

## LlDashPublisher <a href="lldashpublisher" id="lldashpublisher"></a>

| Type          | Name            | Optional | Description                      | Examples |
| ------------- | --------------- | -------- | -------------------------------- | -------- |
| Int           | segmentDuration | N        | Segment duration (unit: seconds) | `3`      |
| List\<String> | crossDomains    | Y        | Cross domain URLs                | `["*"]`  |

## WebrtcPublisher <a href="webrtcpublisher" id="webrtcpublisher"></a>

| Type           | Name    | Optional | Description                 | Examples |
| -------------- | ------- | -------- | --------------------------- | -------- |
| `TimeInterval` | timeout | Y        | ICE timeout (unit: seconds) | `30`     |

## OvtPublisher <a href="ovtpublisher" id="ovtpublisher"></a>

| Type                      | Name | Optional | Description | Examples |
| ------------------------- | ---- | -------- | ----------- | -------- |
| (Reserved for future use) | -    | -        | -           |          |

## FilePublisher <a href="filepublisher" id="filepublisher"></a>

| Type   | Name         | Optional | Description                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             | Examples                                           |
| ------ | ------------ | -------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | -------------------------------------------------- |
| String | filePath     | Y        | <p>A path to store recorded file<br><br>You can use the following macros:<br><code>${TransactionId}: An identifier of transaction</code><br><code>${Id}: An identifier to distinguish files</code><br><code>${StartTime:YYYYMMDDhhmmss}: Start time of recording</code><br><code>${EndTime:YYYYMMDDhhmmss}: End time of of recording</code><br><code>${VirtualHost}: A name of virtual host</code><br><code>${Application}: A name of application</code><br><code>${SourceStream}: A name of input stream</code><br><code>${Stream}: A name of output stream</code><br><code>${Sequence}: A sequence number</code><br><code></code></p> | `"/tmp/${StartTime:YYYYMMDDhhmmss}_${Stream}.mp4"` |
| String | fileInfoPath | Y        | A path of recorded files                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                | `"/tmp/${StartTime:YYYYMMDDhhmmss}_${Stream}.xml"` |

## ThumbnailPublisher <a href="thumbnailpublisher" id="thumbnailpublisher"></a>

| Type          | Name         | Optional | Description       | Examples |
| ------------- | ------------ | -------- | ----------------- | -------- |
| List\<String> | crossDomains | Y        | Cross domain URLs | `["*"]`  |

## OutputProfile <a href="outputprofile" id="outputprofile"></a>

| Type      | Name             | Optional | Description               | Examples                  |
| --------- | ---------------- | -------- | ------------------------- | ------------------------- |
| String    | name             | N        | A name of `OutputProfile` | `"bypass_stream"`         |
| String    | outputStreamName | N        | A name of output stream   | `"${OriginStreamName}"`   |
| `Encodes` | encodes          | Y        |                           | `[Encodes, Encodes, ...]` |

## Encodes <a href="encodes" id="encodes"></a>

| Type          | Name   | Optional | Description | Examples              |
| ------------- | ------ | -------- | ----------- | --------------------- |
| List<`Video`> | videos | Y        |             | `[Video, Video, ...]` |
| List<`Audio`> | audios | Y        |             | `[Audio, Audio, ...]` |
| List<`Image`> | images | Y        |             | `[Image, Image, ...]` |

## Video <a href="video" id="video"></a>

| Type          | Name      | Optional    | Description                                               | Examples                                             |
| ------------- | --------- | ----------- | --------------------------------------------------------- | ---------------------------------------------------- |
| Bool          | bypass    | Y           |                                                           | `true`                                               |
| Enum<`Codec`> | codec     | Conditional | Video codec                                               | `"h264"`                                             |
| Int           | width     | Conditional |                                                           | `1280`                                               |
| Int           | height    | Conditional |                                                           | `720`                                                |
| String        | bitrate   | Conditional | bitrate (You can use "K" or "M" suffix like `100K`, `3M`) | <p><code>"3000000"</code><br><code>"2.5M"</code></p> |
| Float         | framerate | Conditional |                                                           | `29.997`                                             |

## Audio <a href="audio" id="audio"></a>

| Type          | Name       | Optional    | Description                                                 | Examples                                            |
| ------------- | ---------- | ----------- | ----------------------------------------------------------- | --------------------------------------------------- |
| Bool          | bypass     | Y           |                                                             | `true`                                              |
| Enum<`Codec`> | codec      | Conditional | Audio codec                                                 | `"opus"`                                            |
| Int           | samplerate | Conditional |                                                             | `48000`                                             |
| Int           | channel    | Conditional |                                                             | `2`                                                 |
| String        | bitrate    | Conditional | bitrate (You can use "K" or "M" suffix like `128K`, `0.1M`) | <p><code>"128000"</code><br><code>"128K"</code></p> |

## Image <a href="image" id="image"></a>

| Type          | Name      | Optional    | Description                   | Examples          |
| ------------- | --------- | ----------- | ----------------------------- | ----------------- |
| Enum<`Codec`> | codec     | N           |                               | `"jpeg" \| "png"` |
| Int           | width     | Conditional |                               | `854`             |
| Int           | height    | Conditional |                               | `480`             |
| Float         | framerate | N           | An interval of image creation | `1`               |

## Stream <a href="stream" id="stream"></a>

| Type                 | Name    | Optional | Description                      | Examples                            |
| -------------------- | ------- | -------- | -------------------------------- | ----------------------------------- |
| String               | name    | N        | A name of stream                 | `"stream"`                          |
| `InputStream`        | input   | N        | An information of input stream   | `InputStream`                       |
| List<`OutputStream`> | outputs | N        | An information of output streams | `[OutputStream, OutputStream, ...]` |

## NewStream <a href="newstream" id="newstream"></a>

| Type           | Name   | Optional | Description                | Examples       |
| -------------- | ------ | -------- | -------------------------- | -------------- |
| String         | name   | N        | A name of stream to create | `"stream"`     |
| `PullStream`   | pull   | Y        | pull                       | `PullStream`   |
| `MpegtsStream` | mpegts | Y        | Creates a `prestream`      | `MpegtsStream` |

## PullStream <a href="pullstream" id="pullstream"></a>

| Type   | Name | Optional | Description | Examples                     |
| ------ | ---- | -------- | ----------- | ---------------------------- |
| String | url  | N        | URL to pull | `"rtsp://host.com/resource"` |

## InputStream <a href="inputstream" id="inputstream"></a>

| Type          | Name        | Optional | Description                              | Examples                          |
| ------------- | ----------- | -------- | ---------------------------------------- | --------------------------------- |
| String        | agent       | Y        | A name of broadcast tool                 | `"OBS 12.0.4"`                    |
| String        | from        | N        | URI stream created                       | `"tcp://192.168.0.200:33399"`     |
| String        | to          | Y        | URI represents connection with the input | `"rtmp://dev.airensoft.com:1935"` |
| List<`Track`> | tracks      | N        | A list of tracks in input stream         | `[Track, Track, ...]`             |
| `Timestamp`   | createdTime | N        | Creation time                            | `"2020-10-30T11:00:00+09:00"`     |

## OutputStream <a href="outputstream" id="outputstream"></a>

| Type          | Name   | Optional | Description                        | Examples              |
| ------------- | ------ | -------- | ---------------------------------- | --------------------- |
| String        | name   | N        | An name of `OutputStream`          | `"stream_o"`          |
| List<`Track`> | tracks | N        | A list of tracks in `OutputStream` | `[Track, Track, ...]` |

## Track <a href="track" id="track"></a>

| Type              | Name  | Optional    | Description                       | Examples  |
| ----------------- | ----- | ----------- | --------------------------------- | --------- |
| Enum<`MediaType`> | type  | Y           | Media type                        | `"video"` |
| `Video`           | video | Conditional | A configuration of video encoding | `Video`   |
| `Audio`           | audio | Conditional | A configuration of audio encoding | `Audio`   |

## VideoTrack <a href="videotrack" id="videotrack"></a>

| Type              | Name     | Optional | Description | Examples   |
| ----------------- | -------- | -------- | ----------- | ---------- |
| (Extends `Video`) | -        | -        |             |            |
| `Timebase`        | timebase | Y        | Timebase    | `Timebase` |

## Timebase <a href="timebase" id="timebase"></a>

| Type | Name | Optional | Description | Examples |
| ---- | ---- | -------- | ----------- | -------- |
| Int  | num  | N        | Numerator   | `1`      |
| Int  | den  | N        | Denominator | `90000`  |

## AudioTrack <a href="audiotrack" id="audiotrack"></a>

| Type              | Name     | Optional | Description | Examples   |
| ----------------- | -------- | -------- | ----------- | ---------- |
| (Extends `Audio`) | -        | -        |             | `true`     |
| `Timebase`        | timebase | Y        | Timebase    | `Timebase` |

## Record <a href="record" id="record"></a>

| Type                 | Name          | Optional | Description                                              | Examples |
| -------------------- | ------------- | -------- | -------------------------------------------------------- | -------- |
| String               | id            | Y        | Unique identifier                                        |          |
| `OutputStream`       | streams       | N        | A combination of output stream's track name and track id |          |
| Enum<`SessionState`> | state         | N        | Record state                                             |          |
| String               | filePath      | N        | A path of recorded files                                 |          |
| String               | fileInfoPath  | N        | A path of recorded file informations                     |          |
| String               | recordedBytes | N        | Recorded bytes                                           |          |
| Int                  | recordedTime  | N        | Recorded time                                            |          |
| `Timestamp`          | startTime     | N        | Started time                                             |          |
| `Timestamp`          | finishTime    | N        | Finished time                                            |          |
| Int                  | bitrate       | N        | Average bitrate                                          |          |

## Push <a href="push" id="push"></a>

| Type                     | Name             | Optional    | Description                                              | Examples |
| ------------------------ | ---------------- | ----------- | -------------------------------------------------------- | -------- |
| String                   | id               | N           | Unique identifier                                        |          |
| `OutputStream`           | stream           | Y           | A combination of output stream's track name and track id |          |
| Enum<`StreamSourceType`> | protocol         | Y           | Protocol of input stream                                 |          |
| String                   | url              | Y           | Destination URL                                          |          |
| String                   | streamKey        | Conditional | Stream key of destination                                |          |
| Enum<`SessionState`>     | state            | N           | Push state                                               |          |
| Int                      | sentBytes        | N           | Sent bytes                                               |          |
| Int                      | sentPackets      | N           | Sent packets count                                       |          |
| Int                      | sentErrorBytes   | N           | Error bytes                                              |          |
| Int                      | sentErrorPackets | N           | Error packets count                                      |          |
| Int                      | reconnect        | N           | Reconnect count                                          |          |
| `Timestamp`              | startTime        | N           | Started time                                             |          |
| `Timestamp`              | finishTime       | N           | Finished time                                            |          |
| Int                      | bitrate          | N           | Average bitrate                                          |          |

## CommonMetrics <a href="commonmetrics" id="commonmetrics"></a>

| Type        | Name                   | Optional | Description                                                         | Examples                      |
| ----------- | ---------------------- | -------- | ------------------------------------------------------------------- | ----------------------------- |
| `Timestamp` | createdTime            | N        | Creation time                                                       | `"2020-10-30T11:00:00+09:00"` |
| `Timestamp` | lastUpdatedTime        | N        | Modified time                                                       | `"2020-10-30T11:00:00+09:00"` |
| Long        | totalBytesIn           | N        | Received bytes                                                      | `3109481213`                  |
| Long        | totalBytesOut          | N        | Sent bytes                                                          | `1230874123`                  |
| Int         | totalConnections       | N        | Current connections                                                 | `10`                          |
| Int         | maxTotalConnections    | N        | Max connections since the stream is created                         | `293`                         |
| `Timestamp` | maxTotalConnectionTime | N        | When the maximum number of concurrent connections has been updated. | `"2020-10-30T11:00:00+09:00"` |
| `Timestamp` | lastRecvTime           | N        | Last time data was received                                         | `"2020-10-30T11:00:00+09:00"` |
| `Timestamp` | lastSentTime           | N        | Last time data was sent                                             | `"2020-10-30T11:00:00+09:00"` |

## StreamMetrics <a href="streammetrics" id="streammetrics"></a>

| Type                      | Name                   | Optional | Description                            | Examples |
| ------------------------- | ---------------------- | -------- | -------------------------------------- | -------- |
| (Extends `CommonMetrics`) | -                      | -        | Includes all fields of `CommonMetrics` |          |
| `TimeInterval`            | requestTimeToOrigin    | Y        | A elapsed time to connect to Origin    | `1000`   |
| `TimeInterval`            | responseTimeFromOrigin | Y        | A elapsed time from Origin to respond  | `10000`  |
