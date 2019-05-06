# Live Source

OvenMediaEngine \(OME\) currently supports live sources using RTMP, a real-time messaging protocol supported by most popular encoders such as OBS, XSplit, and others. Also, we are developing input protocols such as SRT and MPEG-TS so that OME can support more encoders.

## Configuration

In OME, a component that ingests live sources is called `<Providers>`. You can set it in the configuration as follows:

```markup
<Application>
   <Providers>
      <RTMP>
         ...
      </RTMP>
      <MPEGTS>
         ...
      </MPEGTS>
   </Providers>
<Application>
```

When a live source inputs to the `<Application>`, a stream is automatically created in the `<Application>`. Moreover, the generated stream is streaming through Encoder and Publisher.

## RTMP live stream

If you set up a live stream using an RTMP-based encoder, you need to set the following in `Server.xml`:

```markup
<Application>
   <Providers>
      <RTMP>
         <ListenPort>1935</ListenPort>
         <OverlapStreamProcess>reject</OverlapStreamProcess>
      </RTMP>
   </Providers>
<Application>
```

* `<ListenPort>` is the TCP port that the encoder uses to connect to OME. Also, RTMP uses the 1935/TCP port by default.
* `<OverlapStreamProcess>` is a policy for streams that are inputted as overlaps.

Moreover, `<OverlapStreamProcess>` works with the following rules:

| Value | Description |
| :--- | :--- |
| accept | Accepts a new stream inputted as overlap and disconnects the existing stream. |
| reject | Rejects the new stream inputted as overlap and maintains the existing stream. |

### Publish

If you want to publish the source stream, you need to set the following in the Encoder:

* **`URL`**        `RTMP://[OvenMediaEngine IP]:[RTMP Listen Port]/[App Name]/` 
* **`Stream Key`** `Stream Name`                                                

If you use the default configuration, the `<RTMP><ListenPort>` is 1935, which is the default port for RTMP. So it can be omitted. Also, since the Application named `app` is created by default in the default configuration, you can enter `app` in the `[App Name]`. You can define a Stream Key and use it in the Encoder, and the Streaming URL will change according to the Stream Key.

Moreover, some encoders can include a stream key in the URL, and if you use these encoders, you need to set it as follows:

* **`URL`** `RTMP://[OvenMediaEngine IP]:[RTMP Listen Port]/[App Name]/[Stream Name]` 

### Example with OBS

If you use the default configuration, set the OBS as follows:

![](.gitbook/assets/image%20%282%29.png)

You can set the Stream Key to any name you like at any time.

### Streaming URL

If you use the default configuration, you can stream with the following streaming URLs when you start broadcasting to OBS:

* **`WebRTC`**   `ws://192.168.0.1:3333/app/stream_o`
* **`HLS`**      `http://192.168.0.1:8080/app/stream_o/playlist.m3u8`
* **`MPEG-DASH`**`http://192.168.0.1:8080/app/stream_o/manifest.mpd`

