# WebRTC (Beta)

User can send video/audio from web browser to OvenMediaEngine via WebRTC without plug-in. Of course, you can use any encoder that supports WebRTC transmission as well as a browser.

## Configuration

### Bind

In order for OvenMediaEngine to receive streams through WebRTC, web socket-based signaling port and ICE candidate must be set. The ICE candidate can configure a TCP relay. WebRTC provider and WebRTC publisher can use the same port. Ports of WebRTC provider can be set in  as follows.

```markup
<Bind>
	<Providers>
		...
		<WebRTC>
			<Signalling>
				<Port>3333</Port>
			</Signalling>
			<IceCandidates>
				<TcpRelay>*:3478</TcpRelay>
				<IceCandidate>*:10006-10010/udp</IceCandidate>
			</IceCandidates>
		</WebRTC>
	</Providers>
```

### Application

WebRTC input can be turned on/off for each application. As follows Setting  enables the WebRTC input function of the application.

```markup
<Applications>
	<Application>
		<Name>app</Name>
		<Providers>
			<WebRTC />
```

## URL Pattern

The signaling url for WebRTC input uses the query string**(?direction=send)** as follows to distinguish it from the url for WebRTC playback.

> ws\[s]://\<host>\[:port]/\<app name>/\<stream name>**?direction=send**

When WebRTC transmission starts, a stream is created with  under  application.

### WebRTC over TCP

WebRTC transmission is sensitive to packet loss because it affects all players who access the stream. Therefore, it is recommended to provide WebRTC transmission over TCP. OvenMediaEngine has a built-in TURN server for WebRTC/TCP, and receives or transmits streams using the TCP session that the player's TURN client connects to the TURN server as it is. To use WebRTC/TCP, use transport=tcp query string as in WebRTC playback. See [WebRTC/tcp playback](../streaming/webrtc-publishing.md#webrtc-over-tcp) for more information.

> ws\[s]://\<host>\[:port]/\<app name>/\<stream name>**?direction=send\&transport=tcp**

{% hint style="warning" %}
To use WebRTC/tcp, **<**TcpRelay> must be turned on in \<Bind> setting.
{% endhint %}

## WebRTC Producer

We provide a demo page so you can easily test your WebRTC input. You can access the demo page at the URL below.&#x20;

{% embed url="https://demo.ovenplayer.com/demo_input.html" %}

![](<../.gitbook/assets/image (4).png>)

{% hint style="warning" %}
The getUserMedia API to access the local device only works in a [secure context](https://developer.mozilla.org/en-US/docs/Web/API/MediaDevices/getUserMedia#privacy\_and\_security). So, the WebRTC Input demo page can only work on the https site **** [**https**://demo.ovenplayer.com/demo\_input.html](https://demo.ovenplayer.com/demo\_input.html). This means that due to [mixed content](https://developer.mozilla.org/en-US/docs/Web/Security/Mixed\_content) you have to install the certificate in OvenMediaEngine and use the signaling URL as wss to test this. If you can't install the certificate in OvenMediaEngine, you can temporarily test it by allowing the insecure content of the demo.ovenplayer.com URL in your browser.
{% endhint %}

### Custom WebRTC Producer

To create a custom WebRTC Producer, you need to implement OvenMediaEngine's Signaling Protocol. The protocol is structured in a simple format and uses the[ same method as WebRTC Streaming](../streaming/webrtc-publishing.md#signalling-protocol).

![](<../.gitbook/assets/image (10).png>)

When the player connects to ws\[s]://host:port/app/stream?**direction=send** through a web socket and sends a request offer command, the server responds to the offer sdp. If transport=tcp exists in the query string of the URL, [iceServers ](https://developer.mozilla.org/en-US/docs/Web/API/RTCIceServer)information is included in offer sdp, which contains the information of OvenMediaEngine's built-in TURN server, so you need to set this in RTCPeerConnection to use WebRTC/TCP. The player then setsRemoteDescription and addIceCandidate offer sdp, generates an answer sdp, and responds to the server.
