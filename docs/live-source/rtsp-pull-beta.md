# RTSP Pull

From version 0.10.4, RTSP Pull input is supported as a beta version. The supported codecs are H.264, AAC(ADTS). Supported codecs will continue to be added.&#x20;

This function pulls a stream from an external RTSP server and operates as an RTSP client.&#x20;

### Configuration

RTSP Pull is provided through OriginMap configuration. OriginMap is the rule that the Edge server pulls the stream of the Origin server. Edge server can pull a stream of origin with RTSP and OVT (protocol defined by OvenMediaEngine for Origin-Edge) protocol. See the [Clustering](../origin-edge-clustering.md) section for more information about OVT.

```markup
<VirtualHosts>
		<VirtualHost include="VHost*.xml" />
		<VirtualHost>
			<Name>default</Name>

			<Host>
				<Names>
					<!-- Host names
						<Name>stream1.airensoft.com</Name>
						<Name>stream2.airensoft.com</Name>
						<Name>*.sub.airensoft.com</Name>
						<Name>192.168.0.1</Name>
					-->
					<Name>*</Name>
				</Names>
				<!--
				<TLS>
					<CertPath>path/to/file.crt</CertPath>
					<KeyPath>path/to/file.key</KeyPath>
					<ChainCertPath>path/to/file.crt</ChainCertPath>
				</TLS>
				-->
			</Host>
			
			<Origins>
				<Origin>
					<Location>/app_name/rtsp_stream_name</Location>
					<Pass>
						<Scheme>rtsp</Scheme>
						<Urls><Url>192.168.0.200:554/</Url></Urls>
					</Pass>
				</Origin>
			</Origins>
		</VirtualHost>
	</VirtualHosts>
```

For example, in the above setup, when a player requests "ws://ome.com/**app\_name/rtsp\_stream\_name"** to stream WebRTC, it pulls the stream from "rtsp://**192.168.0.200:554"** and publishes it to WebRTC.

{% hint style="warning" %}
If the app name set in Location isn't created, OvenMediaEngine creates the app with default settings. The default generated app doesn't have an OPUS encoding profile, so to use WebRTC streaming, you need to add the app to your configuration.
{% endhint %}

### Publish

The pull-type provider is activated by the publisher's streaming request. And if there is no client playing for 30 seconds, the provider is automatically disabled.

According to the above setting, the RTSP pull provider operates for the following streaming URLs.

| Protocol | URL                                                               |
| -------- | ----------------------------------------------------------------- |
| WebRTC   | ws:://ome.com:3333/app\_name/rtsp\_stream\_name                   |
| HLS      | http://ome.com:8080/app\_name/rtsp\_stream\_name/playlist.m3u8    |
| DASH     | http://ome.com:8080/app\_name/rtsp\_stream\_name/manifest.mpd     |
| LL DASH  | http://ome.com:8080/app\_name/rtsp\_stream\_name/manifest\_ll.mpd |

