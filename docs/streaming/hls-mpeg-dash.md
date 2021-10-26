# Low-Latency DASH and HLS streaming

OvenMediaEngine supports HTTP based streaming protocols such as HLS, MPEG-DASH(Hereafter DASH), and Low-Latency MPEG-DASH with CMAF(Hereafter LLDASH). &#x20;

| Title    | Functions                                                            |
| -------- | -------------------------------------------------------------------- |
| Delivery | <p>HTTP for HLS and DASH<br>HTTP 1.1 chunked transfer for LLDash</p> |
| Security | TLS (HTTPS)                                                          |
| Format   | <p>TS for HLS </p><p>ISOBMFF for DASH</p><p>CMAF for LLDASH</p>      |
| Codec    | H.264, AAC                                                           |

{% hint style="info" %}
OvenMediaEngine will support Low-Latency HLS in the near future. We are always keeping an eye on the decision from Apple inc.
{% endhint %}

## Configuration

To use HLS, Dash and LLDash, you need to add the `<HLS>` and`<DASH>` elements to the `<Publishers>` in the configuration as shown in the following example.

```markup
<Server version="8">
    <Bind>
        <Publishers>
            <HLS>
                <Port>80</Port>
            </HLS>
            <DASH>
                <Port>80</Port>
            </DASH>
        </Publishers>
    </Bind>
    <VirtualHosts>
		    <VirtualHost>
            <Applications>
                <Application>
                    <Publishers>
                        <HLS>
                            <SegmentDuration>5</SegmentDuration>
                            <SegmentCount>3</SegmentCount>
                            <CrossDomains>
                                <Url>*<Url>
                            </CrossDomains>
                        </HLS>
                        <DASH>
                            <SegmentDuration>5</SegmentDuration>
                            <SegmentCount>3</SegmentCount>
                            <CrossDomains>
                                <Url>*<Url>
                            </CrossDomains>
                        </DASH>
                        <LLDASH>
                            <SegmentDuration>5</SegmentDuration>
                            <CrossDomains>
                                <Url>*<Url>
                            </CrossDomains>
                        </LLDASH>
                    </Publishers>
                </Application>
            </Applications>
        </VirtualHost>
    </VirtualHosts>
</Server>
```

| Element         | Decscription                                                                                                                                                                                                                                                                                                                                                                        |
| --------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Bind            | Set the HTTP port to provide HLS and DASH. LLDASH and DASH are provided on the same port, and DASH and HLS can be set to the same port.                                                                                                                                                                                                                                             |
| SegmentDuration | Set the length of the segment in seconds. The shorter the `<SegmentDuration>`, the lower latency can be streamed, but it is less stable during streaming. Therefore, we are recommended to set it to 3 to 5 seconds.                                                                                                                                                                |
| SegmentCount    | <p>Set the number of segments to be exposed to <code>*.mpd</code>. The smaller the number of <code>&#x3C;SegmentCount></code>, the lower latency can be streamed, but it is less reliable during streaming. Therefore, it is recommended to set this value to 3. </p><p>It doesn't need to set SegmentCount for LLDASH because LLDASH only has one segment on OvenMediaEngine. </p> |
| CrossDomains    | Control the domain in which the player works through `<CorssDomain>`. For more information, please refer to the [CrossDomain](hls-mpeg-dash.md#crossdomain) section.                                                                                                                                                                                                                |

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

LLDASH, DASH, and HLS Streaming are ready when a live source is inputted and a stream is created. Viewers can stream using OvenPlayer or other players.

Also, you need to set H.264 and AAC in the Transcoding profile because MPEG-DASH and HLS use these codecs.

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

When you create a stream, as shown above, you can play LLDASH, DASH, and HLS through OvenPlayer with the following URL:

| Protocol      | URL format                                                                           |
| ------------- | ------------------------------------------------------------------------------------ |
| LLDASH        | `http://<Server IP>[:<DASH Port>]/<Application Name>/<Stream Name>/manifest_ll.mpd`  |
| Secure LLDASH | `https://<Domain>[:<DASH TLSPort>]/<Application Name>/<Stream Name>/manifest_ll.mpd` |
| DASH          | `http://<Server IP>[:<DASH Port>]/<Application Name>/<Stream Name>/manifest.mpd`     |
| Secure DASH   | `https://<Domain>[:<DASH TLSPort>]/<Application Name>/<Stream Name>/manifest.mpd`    |
| HLS           | `http://<Server IP>[:<HLS Port>]/<Application Name>/<Stream Name>/playlist.m3u8`     |
| Secure HLS    | `https://<Domain>[:<HLS TLSPort>]/<Application Name>/<Stream Name>/playlist.m3u8`    |

If you use the default configuration, you can start streaming with the following URL:

* `https://[OvenMediaEngine IP]:443/app/<stream name>_o/playlist.m3u8`
* `http://[OvenMediaEngine IP]:80/app/<stream name>_o/playlist.m3u8`
* `https://[OvenMediaEngine IP]:443/app/<stream name>_o/manifest.mpd`
* `http://[OvenMediaEngine IP]:80/app/<stream name>_o/manifest.mpd`
* `https://[OvenMediaEngine IP]:443/app/<stream name>_o/manifest_ll.mpd`
* `http://[OvenMediaEngine IP]:80/app/<stream name>_o/manifest_ll.mpd`

We have prepared a test player that you can quickly see if OvenMediaEngine is working. Please refer to the [Test Player](../test-player.md) for more information.
