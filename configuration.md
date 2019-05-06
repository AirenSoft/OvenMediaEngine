# Configuration

OvenMediaEngine \(OME\) has an XML configuration file. Therefore, when the server is running, it reads and loads the configuration file from the following location: 

```text
/<OvenMediaEngine Binary Path>/conf/Server.xml
```

You can make detailed settings for `<Host>` and `<Application>` in the `Server.xml` configuration file.

## Host

`<Host>` means Virtual Host. It allows using the isolate the `<Host>` from `<IP>` or `<Domain>`. You can use the following in the configuration file:

```markup
<?xml version="1.0" encoding="UTF-8"?>
<Server version="1">
    <Hosts>
        <Host>
            <IP>192.168.0.1</IP>
            ...
        </Host>
        <Host>
            <IP>192.168.0.2</IP>
            ...
        </Host>
    </Hosts>
</Server>
```

{% hint style="info" %}
We are currently testing features that set up multiple virtual hosts, and It will be updated on OME soon.
{% endhint %}

## Application

`<Application>` consists of various elements that can define the operation of the stream including Stream input, Encoding, and Stream output. In other words, you can create as many `<Application>` as you want and build a variety of streaming environments.

```markup
<Host>
    ...
    <Applications>
        <Application>
            ...
        </Application>
        <Application>
            ...
        </Application>
    </Applications>
</Host>
```

`<Application>` needs to set `<Name>` and `<Type>` as follows:

```markup
<Application>
    <Name>app</Name>
    <Type>live</Type>
    ...
</Application>
```

* `<Name>` is used to configure the Streaming URL. 
* `<Type>` defines the operation of the `<Application>`. Currently, there are the following types:

| Application Type | Description |
| :--- | :--- |
| live | Application works in Single Mode or Origin Mode. |
| liveedge | Edge Application receives a stream from Origin and retransmits it. |

### Origin

The `<Type>` in `<Application>` is "live", it can operate in Single mode or in Origin mode. Also, If you want to configure the Cluster as an Origin-Edge, you need to set the Origin server as follows:

```markup
<Origin>
    <ListenPort>9000</ListenPort>
</Origin>
```

If you set up the `<ListenProt>` on the Origin server as above, it uses 9000/UDP port to wait for the Edge connection. For more information about Origin-Edge, see the  [Origin-Edge Clustering](origin-edge-clustering.md) chapter.

### Providers

`<Providers>` is the same as Ingest because it creates a stream when there is an input to the `<Providers>`. Moreover, It currently supports RTMP input and you can set the port and overlap stream process as follows:

```markup
<Application>
   <Providers>
      <RTMP>
         <ListenPort>1935</ListenPort>
         <OverlapStreamProcess>reject</OverlapStreamProcess>
      </RTMP>
   </Providers>
</Application>
```

If you want to get more information about the `<Providers>`, please refer to the [Live Source](live-source.md) chapter.

### Encodes & Stream

`<Encode>` has profiles that encode the Input Stream coming in via `<Providers>`. You can available this only if The `<Type>` in `<Application>` is "live". Also, you can set up multiple profiles in `<Encode>`.

```markup
<Application>
    <Encodes>
        <Encode>
            <Name>HD_VP8_OPUS</Name>
            <Video>
                <Codec>vp8</Codec>
                ...
            </Video>
            <Audio>
                <Codec>opus</Codec>
                ...
            </Audio>
        </Encode>
        <Encode>
            ...
        </Encode>
    </Encodes>
</Application>
```

You can create a new Output Stream by grouping the transcoded results in the `<Stream>` setting. 

* `<Name>` is the Stream Name when you use to configure the Streaming URL.
* `<Stream>` can be combined with multiple `<Profile>` and output as Multi Codec or Adaptive.
* `<Profile>` is `<Encode><Name>` and maps the Transcoding profile. 

```markup
<Application>
   <Streams>
      <Stream>
         <Name>${OriginStreamName}_o</Name>
         <Profiles>
            <Profile>HD_VP8_OPUS</Profile>
            <Profile>HD_H264_AAC</Profile>
         </Profiles>
      </Stream>
   </Streams>
</Application>
```

For more information about the Encodes and Streams, please see the [Transcoding](transcoding.md) chapter.

### Publishers

You can configure the Output Stream operation in `<Publishers>`. 

* `<ThreadCount>` is the number of threads used by each component responsible for the `<Publishers>` protocol.

{% hint style="info" %}
You need many threads to transmit streams to a large number of users at the same time. Therefore, we are recommended to set `<ThreadCount>` equal to the number of CPU cores.
{% endhint %}

```markup
<Application>
   <Publishers>
      <ThreadCount>2</ThreadCount>
      <HLS>
         ...
      </HLS>
      <DASH>
         ...
      </DASH>
      <WebRTC>
         ...
         <Signalling>
            <ListenPort>3333</ListenPort>
         </Signalling>
      </WebRTC>
   </Publishers>
</Application>
```

​OME currently supports WebRTC, HLS, and MPEG-DASH as the `<Publishers>`. If you do not use any protocol and you can delete that protocol setting, the component for that protocol is not initialized. As a result, you can save system resources by deleting settings for unused protocol components.

If you want to learn more about WebRTC, please visit the [WebRTC Streaming](webrtc-publishing.md) chapter. And if you want to get more information about HLS and MPEG-DASH, please refer to the chapter on [HLS & MPEG-DASH Streaming](hls-mpeg-dash.md).

## ConfigExample

Finally, `Server.xml` will be configured as follows:

```markup
<?xml version="1.0" encoding="UTF-8"?>
<Server version="1">
   <Name>OvenMediaEngine</Name>
   <Hosts>
      <Host>
         <Name>default</Name>
         <IP>*</IP>
         <Applications>
            <Application>
               <Name>app</Name>
               <Type>live</Type>
               <Origin>
                  <ListenPort>9000</ListenPort>
               </Origin>
               <Encodes>
                  <Encode>
                     <Name>HD_VP8_OPUS</Name>
                     <Audio>
                        <Codec>opus</Codec>
                        <Bitrate>128000</Bitrate>
                        <Samplerate>48000</Samplerate>
                        <Channel>2</Channel>
                     </Audio>
                     <Video>
                        <!-- vp8, h264 -->
                        <Codec>vp8</Codec>
                        <Width>1280</Width>
                        <Height>720</Height>
                        <Bitrate>2000000</Bitrate>
                        <Framerate>30.0</Framerate>
                     </Video>
                  </Encode>
                  <Encode>
                     <Name>HD_H264_AAC</Name>
                     <Audio>
                        <Codec>aac</Codec>
                        <Bitrate>128000</Bitrate>
                        <Samplerate>48000</Samplerate>
                        <Channel>2</Channel>
                     </Audio>
                     <Video>
                        <Codec>h264</Codec>
                        <Width>1280</Width>
                        <Height>720</Height>
                        <Bitrate>2000000</Bitrate>
                        <Framerate>30.0</Framerate>
                     </Video>
                  </Encode>
               </Encodes>
               <Streams>
                  <Stream>
                     <Name>${OriginStreamName}_o</Name>
                     <Profiles>
                        <Profile>HD_VP8_OPUS</Profile>
                        <Profile>HD_H264_AAC</Profile>
                     </Profiles>
                  </Stream>
               </Streams>
               <Providers>
                  <RTMP>
                     <ListenPort>1935</ListenPort>
                     <OverlapStreamProcess>reject</OverlapStreamProcess>
                  </RTMP>
               </Providers>
               <Publishers>
                  <ThreadCount>2</ThreadCount>
                  <HLS>
                     <ListenPort>8080</ListenPort>
                     <SegmentDuration>5</SegmentDuration>
                     <SegmentCount>3</SegmentCount>
                     <CrossDomain>
                        <Url>*</Url>
                     </CrossDomain>
                  </HLS>
                  <DASH>
                     <ListenPort>8080</ListenPort>
                     <SegmentDuration>5</SegmentDuration>
                     <SegmentCount>3</SegmentCount>
                     <CrossDomain>
                        <Url>airensoft.com</Url>
                     </CrossDomain>
                  </DASH>
                  <WebRTC>
                     <IceCandidates>
                        <IceCandidate>*:10000-10005/udp<IceCandidate>
                     </IceCandidates>
                     <Timeout>30000</Timeout>
                     <Signalling>
                        <ListenPort>3333</ListenPort>
                     </Signalling>
                  </WebRTC>
               </Publishers>
            </Application>
         </Applications>
      </Host>
   </Hosts>
</Server>
```

 ****

