# Low-Latency HLS

Apple supports Low-Latency HLS (LLHLS), which enables low-latency video streaming while maintaining scalability. LLHLS enables broadcasting with an end-to-end latency of about 2 to 5 seconds. OvenMediaEngine officially supports LLHLS as of v0.14.0.&#x20;

LLHLS is an extension of HLS, so legacy HLS players can play LLHLS streams. However, the legacy HLS player plays the stream without using the low-latency function.

| Title     | Descriptions              |
| --------- | ------------------------- |
| Delivery  | <p>HTTP/1.1<br>HTTP/2</p> |
| Security  | TLS (HTTPS)               |
| Container | fMP4                      |
| Codecs    | <p>H.264<br>AAC</p>       |

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
| CrossDomains    | Control the domain in which the player works through `<CorssDomain>`. For more information, please refer to the [CrossDomain](hls-mpeg-dash.md#crossdomain) section.                                                                       |

{% hint style="info" %}
HTTP/2 outperforms HTTP/1.1, especially with LLHLS. Since all current browsers only support h2, HTTP/2 is supported only on TLS port. Therefore, it is highly recommended to use LLHLS on the TLS port.
{% endhint %}

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

We have prepared a test player that you can quickly see if OvenMediaEngine is working. Please refer to the [Test Player](../test-player.md) for more information.
