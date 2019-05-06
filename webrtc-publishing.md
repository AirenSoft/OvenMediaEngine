# WebRTC Streaming

OvenMediaEngine \(OME\) uses WebRTC to provide Ultra-Low Latency Streaming. WebRTC uses RTP for media transmission and provides various extensions.

OME currently offers the following features:

| Title | Functions |
| :--- | :--- |
| Delivery | RTP / RTCP |
| Security | DTLS,  SRTP |
| Connectivity | ICE |
| Error Correction | ULPFEC \(VP8, H.264\), In-band FEC \(Opus\) |
| Codec | VP8, H.264, Opus |
| Signalling | Self-Defined Signalling Protocol and Embedded Web Socket-Based Server |

## Configuration

If you want to use the WebRTC feature, you need to add `<WebRTC>` element to the `<Publishers>` in the `Server.xml` configuration file, as shown in the example below.

{% code-tabs %}
{% code-tabs-item title="Server.xml" %}
```markup
<Application>
   <Publishers>
      <WebRTC>
      ...
      </WebRTC>
   </Publishers>
</Application>
```
{% endcode-tabs-item %}
{% endcode-tabs %}

## ICE

WebRTC uses ICE for peer-to-peer connections and specifically NAT traversal. The web browser or player recognizes OME as a peer and exchanges the Ice Candidate with each other in the Signalling phase. Therefore, OME also requires an Ice Candidate extraction setting.

When you set as follows, OME randomly selects one of the IPs that can be extracted from the server and one of the UDP ports between 10000 and 10005, makes it `<IceCandidate>`, and passes it to the peer. However, if you want to use only one specific IP to Ice Candidate, you can set it like `192.168.0.1:10000-1005/udp`.

```markup
<Publishers>
   <WebRTC>
      <IceCandidate>*:10000-10005/udp</IceCandidate>   
      <Timeout>30000</Timeout>
   </WebRTC>
</Publishers>
```

OME periodically sends a `STUN request` and receives a `STUN response` to check the connection status for NAT traversal. Some browsers also perform the same tasks as OME.

At this time, OME checks time to wait for the response according to the value set in `<Timeout>`, and if the response does not come within the specified time, OME will automatically determine that the connection has been lost and clean up the session.

## Signalling

WebRTC requires Signalling to exchange Offer SDP and Answer SDP, but this part is not standardized. If you want to use SDP, you need to create your exchange protocol yourself. However, OME has embedded a WebSocket-based Signalling server and provides our defined Signalling protocol. Also, OvenPlayer supports our Signalling protocol.

You need to set `<WebRTC>` in `<Publishers>` to use the Signalling protocol.

{% code-tabs %}
{% code-tabs-item title="Server.xml" %}
```markup
<Publishers>
   <WebRTC>
      <IceCandidates>
         <IceCandidate>*:10000-10005/udp</IceCandidate>   
      </IceCandidates>
      <!-- STUN timeout (milliseconds) -->
      <Timeout>30000</Timeout>
      <Signalling>
         <ListenPort>3333</ListenPort>
      </Signalling>
   </WebRTC>
</Publishers>
```
{% endcode-tabs-item %}
{% endcode-tabs %}

### WebSocket over TLS

Most browsers have applied TLS as an essential factor when using WebSockets on a page that has HTTPS applied, and only can connect and work using `wss://` according to policy.

Therefore, if you have applied OvenPlayer to a page that has HTTPS based, the Signalling server of OME need to use TLS, and you can configure it as follows:.

{% code-tabs %}
{% code-tabs-item title="Server.xml" %}
```markup
<Signalling>
    <ListenPort>3333</ListenPort>
    <TLS>
        <CertPath>/path/to/cert/filename.crt</CertPath>
        <KeyPath>/path/to/cert/filename.key</KeyPath>
	</TLS>
</Signalling>
```
{% endcode-tabs-item %}
{% endcode-tabs %}

If you specify the location of the `*.CRT` and `*.KEY` file, TLS is applied to the Signalling server, and you can connect using `wss://`.

### Signalling Protocol

The Signalling protocol is defined in a simple way:

![](.gitbook/assets/image%20%283%29.png)

If you want to use a player other than OvenPlayer, you need to develop the Signalling protocol as shown above and can integrate OME.

## Streaming

WebRTC Streaming starts when a live source is inputted and a stream is created. Viewers can stream using OvenPlayer or players that have developed or applied the OvenMediaEngine Signalling protocol.

Also, the codecs supported by each browser are different, so you need to set the Transcoding profile according to the browser you want to support. For example, Safari for iOS supports H.264 but not VP8. If you want to support all browsers, please set up VP8, H.264 and Opus codecs in all transcoders.

```markup
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
```

{% hint style="info" %}
Some browsers support both H.264 and VP8 to send Answer SDP to OME, but sometimes H.264 cannot be played. In this situation, if you write the VP8 above the H.264 code line in the Transcoding profile setting, you can increase the priority of the VP8.

Using this manner so that some browsers, support H.264 but cannot be played, can stream smoothly using VP8. This means that you can solve most problems with this method.
{% endhint %}

If you created a stream as shown in the table above, you can play WebRTC on OvenPlayer via the following URL:

| Protocol | URL format |
| :--- | :--- |
| WebRTC Signalling | http://&lt;OME Server IP&gt;\[:&lt;OME Signalling Port\]/&lt;Application name&gt;/&lt;Stream name&gt; |
| Secure WebRTC Signalling | https://&lt;OME Server IP&gt;\[:&lt;OME Signalling Port\]/&lt;Application name&gt;/&lt;Stream name&gt; |

If you use the default configuration, you can stream to the following URL:

* `ws://[OvenMediaEngine IP]:3333/app/stream_o`
* `wss://[OvenMediaEngine IP]:3333/app/stream_o`

We have prepared a test player so that you can easily check that the OvenMediaEngine is working well. For more information, see the[ Test Player](test-player.md) chapter.



