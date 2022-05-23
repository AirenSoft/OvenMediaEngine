# TLS Encryption

Most browsers can't load resources via HTTP and WS (WebSocket) from HTTPS web pages secured with TLS. Therefore, if the player is on an HTTPS page, the player must request streaming through "https" and "wss" URLs secured with TLS. In this case, you must apply the TLS certificate to the OvenMediaEngine.

You can set the port for TLS in `TLSPort`. Currently, LLHLS and WebRTC Signaling support TLS.

```markup
<Bind>
	...

	<Publishers>
	...
		<LLHLS>
			<Port>80</Port>
			<TLSPort>443</TLSPort>
		</LLHLS>
		<WebRTC>
			<Signalling>
				<Port>3333</Port>
				<TLSPort>3334</TLSPort>
			</Signalling>
			...
		</WebRTC>
	</Publishers>
</Bind>
```

Add your certificate files to  as follows:

```markup
<Domain>
	<Names>
		<Name>*.airensoft.com</Name>
	</Names>
	<TLS>
		<CertPath>path/to/file.crt</CertPath>
		<KeyPath>path/to/file.key</KeyPath>
		<ChainCertPath>path/to/file.crt</ChainCertPath>
	</TLS>
</Domain>
```

To configure HTTPs for HLS, DASH, and WebRTC Signalling servers, the TLS element must be enabled. The `CertPath` has to indicate a server certificate and the `KeyPath` has to indicate a private key file. They can be set to absolute paths or relative paths from the executable. If the server certificate is issued using an intermediate certificate, some browsers may complain about a certificate. In this case, you should set a bundle of chained certificates provided by a Certificate Authority in `ChainCertPath`.

If you set up TLS, you can't set IP or \* into \<Name>. You can only set Domains that the certificate contains. If you have a certificate for `*.host.com`, it means you can set domains such as `aaa.host.com`, `bbb.host.com`, and `*.host.com`.

If the certificate settings are completed correctly, WebRTC streaming can be played `wss://url` with HLS and DASH streaming `https://url`.

