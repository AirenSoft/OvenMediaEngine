# TLS Encryption

Most browsers can't load resources via HTTP and WS (WebSocket) from HTTPS web pages secured with TLS. Therefore, if the player is on an HTTPS page, the player must request streaming through "https" and "wss" URLs secured with TLS. In this case, you must apply the TLS certificate to the OvenMediaEngine.

### Docker
To link certificates from your Docker host, uncomment the example in the Docker compose file or manually connect a volume in the Docker run command, e.g. -v ~/local/cert/path:/opt/ovenmediaengine/bin/certs
You can set the port for TLS in `TLSPort`. Currently, LLHLS and WebRTC Signaling support TLS.

```markup
<Bind>
    <!-- For API Server -->
    <Managers>
	    <API>
	    <Port>8081</Port>
	    <TLSPort>8082</TLSPort>
            <WorkerCount>1</WorkerCount>
	</API>
    </Managers>
    <!-- For Providers -->
    <Providers>
        <WebRTC>
	    <Signalling>
		<Port>3333</Port>
		<TLSPort>3334</TLSPort>
		<WorkerCount>1</WorkerCount>
	    </Signalling>
            ...
        </WebRTC>
    </Providers>
    <!- For Publishers -->
    <Publishers>
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

<pre class="language-markup"><code class="lang-markup">&#x3C;!-- For API Server -->
&#x3C;Managers>
  &#x3C;Host>
    &#x3C;Names>
      &#x3C;Name>*&#x3C;/Name>
    &#x3C;/Names>
    &#x3C;TLS>
      &#x3C;CertPath>path/to/file.crt&#x3C;/CertPath>
      &#x3C;KeyPath>path/to/file.key&#x3C;/KeyPath>
      &#x3C;ChainCertPath>path/to/file.crt&#x3C;/ChainCertPath>
    &#x3C;/TLS>
  &#x3C;/Host>
  ...
&#x3C;/Managers>

<strong>&#x3C;VirtualHosts>
</strong>  &#x3C;VirtualHost>
    &#x3C;!-- For Vitual Host -->
    &#x3C;Host>
        &#x3C;Names>
            &#x3C;Name>*&#x3C;/Name>
        &#x3C;/Names>
        &#x3C;TLS>
          &#x3C;CertPath>/etc/pki/airensoft.com/_airensoft_com.crt&#x3C;/CertPath>
          &#x3C;KeyPath>/etc/pki/airensoft.com/_airensoft_com.key&#x3C;/KeyPath>
          &#x3C;ChainCertPath>/etc/pki/airensoft.com/_airensoft_com.ca-bundle&#x3C;/ChainCertPath>
        &#x3C;/TLS>
    &#x3C;/Host>
</code></pre>

To enable HTTP for HLS and WebRTC signaling servers, you must enable the TLS element and install the certificate file in PEM format. This involves indicating a server certificate through the `CertPath`, as well as a private key file through the `KeyPath`. These paths can be specified as either absolute or relative paths from the executable. However, if the server certificate was issued using an intermediate certificate, some browsers may raise concerns about the certificate's authenticity. To address this, a bundle of chained certificates provided by a Certificate Authority can be set in the `ChainCertPath`.

Assuming the certificate settings are correctly configured, WebRTC streaming can then be played via the wss://url protocol, while LLHLS streaming can be accessed via [https://url](https://url/).

### Let's Encrypt, 
If you used certbot to create your certificates, the PEM files it creates can be linked in your Server.xml like this:

```markup
			<!-- Settings for multi ip/domain and TLS -->
			<Host>
				<Names>
					<Name>example.com</Name>
				</Names>
				<TLS>
					<CertPath>/opt/ovenmediaengine/bin/cert/live/example.com/cert.pem</CertPath>
					<KeyPath>/opt/ovenmediaengine/bin/cert/live/example.com/privkey.pem</KeyPath>
					<ChainCertPath>/opt/ovenmediaengine/bin/cert/live/example.com/chain.pem</ChainCertPath>
				</TLS>
			</Host>
```


