# WebRTC / WHIP

User can send video/audio from web browser to OvenMediaEngine via WebRTC without plug-in. Of course, you can use any encoder that supports WebRTC transmission as well as a browser.

## Configuration

### Bind

OvenMediaEngine supports self-defined signaling protocol and [WHIP ](https://datatracker.ietf.org/doc/draft-ietf-wish-whip/)for WebRTC ingest.&#x20;

```markup
<Bind>
    <Providers>
        ...
        <WebRTC>
            <Signalling>
                <Port>3333</Port>
                <TLSPort>3334</TLSPort>
            </Signalling>
            <IceCandidates>
                <TcpRelay>*:3478</TcpRelay>
                <TcpForce>false</TcpForce>
                <IceCandidate>*:10000-10005/udp</IceCandidate>
            </IceCandidates>
        </WebRTC>
    </Providers>
```

You can set the port to use for signaling in `<Bind><Provider><WebRTC><Signaling>`. `<Port>` is for setting an unsecured HTTP port, and `<TLSPort>` is for setting a secured HTTP port that is encrypted with TLS.&#x20;

For WebRTC ingest, you must set the ICE candidates of the OvenMediaEnigne server to `<IceCandidates>`. The candidates set in `<IceCandate>` are delivered to the WebRTC peer, and the peer requests communication with this candidate. Therefore, you must set the IP that the peer can access. If the IP is specified as \*, OvenMediaEngine gathers all IPs of the server and delivers them to the peer.

`<TcpRelay>` means OvenMediaEngine's built-in TURN Server. When this is enabled, the address of this turn server is passed to the peer via self-defined signaling protocol or WHIP, and the peer communicates with this turn server over TCP. This allows OvenMediaEngine to support WebRTC/TCP itself. For more information on URL settings, check out [WebRTC over TCP](webrtc.md#webrtc-over-tcp).

### Application

WebRTC input can be turned on/off for each application. As follows Setting enables the WebRTC input function of the application. The `<CrossDomains>` setting is used in WebRTC signaling.

```markup
<Applications>
    <Application>
        <Name>app</Name>
        <Providers>
            <WebRTC>
                <Timeout>30000</Timeout>
                <CrossDomains>
                    <Url>*</Url>
                </CrossDomains>
            </WebRTC>
```

## URL Pattern

OvenMediaEnigne supports self-defined signaling protocol and WHIP for WebRTC ingest.

### Self-defined Signaling URL

The signaling URL for WebRTC ingest uses the query string `?direction=send` as follows to distinguish it from the url for WebRTC playback. Since the self-defined WebRTC signaling protocol is based on WebSocket, you must specify ws\[s] as the scheme.

> ws\[s]://\<host>\[:signaling port]/\<app name>/\<stream name>**?direction=send**

### WHIP URL

For ingest from the WHIP client, put `?direction=whip` in the query string in the signaling URL as in the example below. Since WHIP is based on HTTP, you must specify http\[s] as the scheme.

> http\[s]://\<host>\[:signaling port]/\<app name>/\<stream name>**?direction=whip**

### WebRTC over TCP

WebRTC transmission is sensitive to packet loss because it affects all players who access the stream. Therefore, it is recommended to provide WebRTC transmission over TCP. OvenMediaEngine has a built-in TURN server for WebRTC/TCP, and receives or transmits streams using the TCP session that the player's TURN client connects to the TURN server as it is. To use WebRTC/TCP, use transport=tcp query string as in WebRTC playback. See [WebRTC/tcp playback](../streaming/webrtc-publishing.md#webrtc-over-tcp) for more information.

> ws\[s]://\<host>\[:port]/\<app name>/\<stream name>**?direction=send\&transport=tcp**
>
> http\[s]://\<host>\[:port]/\<app name>/\<stream name>**?direction=whip\&transport=tcp**

{% hint style="warning" %}
To use WebRTC/tcp, `<TcpRelay>` must be turned on in `<Bind>` setting.&#x20;

If `<TcpForce>` is set to true, it works over TCP even if you omit the `?transport=tcp` query string from the URL.
{% endhint %}

## WebRTC Producer

We provide a demo page so you can easily test your WebRTC input. You can access the demo page at the URL below.

{% embed url="https://demo.ovenplayer.com/demo_input.html" %}

![](<../.gitbook/assets/image (4) (1).png>)

{% hint style="warning" %}
The getUserMedia API to access the local device only works in a [secure context](https://developer.mozilla.org/en-US/docs/Web/API/MediaDevices/getUserMedia#privacy\_and\_security). So, the WebRTC Input demo page can only work on the https site \*\*\*\* [**https**://demo.ovenplayer.com/demo\_input.html](https://demo.ovenplayer.com/demo\_input.html). This means that due to [mixed content](https://developer.mozilla.org/en-US/docs/Web/Security/Mixed\_content) you have to install the certificate in OvenMediaEngine and use the signaling URL as wss to test this. If you can't install the certificate in OvenMediaEngine, you can temporarily test it by allowing the insecure content of the demo.ovenplayer.com URL in your browser.
{% endhint %}

### Self-defined WebRTC Ingest Signaling Protocol

To create a custom WebRTC Producer, you need to implement OvenMediaEngine's Self-defined Signaling Protocol or WHIP. Self-defined protocol is structured in a simple format and uses the[ same method as WebRTC Streaming](../streaming/webrtc-publishing.md#signalling-protocol).

![](<../.gitbook/assets/image (10).png>)

When the player connects to ws\[s]://host:port/app/stream?**direction=send** through a web socket and sends a request offer command, the server responds to the offer sdp. If transport=tcp exists in the query string of the URL, [iceServers ](https://developer.mozilla.org/en-US/docs/Web/API/RTCIceServer)information is included in offer sdp, which contains the information of OvenMediaEngine's built-in TURN server, so you need to set this in RTCPeerConnection to use WebRTC/TCP. The player then setsRemoteDescription and addIceCandidate offer sdp, generates an answer sdp, and responds to the server.
