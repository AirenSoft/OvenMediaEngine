# REST API

## Overview

The REST APIs provided by OME allow you to query or change settings such as VirtualHost and Application/Stream.

{% hint style="warning" %}
There are some limitations/considerations.

* If you add/change/delete the settings of the App/Output Profile by invoking the API, the app will be restarted. This means that all sessions connected to the app will be disconnected.
* VirtualHost settings in Server.xml cannot be modified through API. This rule also applies to Application/OutputStream, etc. within that VirtualHost. So, if you call a POST/PUT/DELETE API for VirtualHost/Application/OutputProfile declared in Server.xml, it will not work with a 403 Forbidden error.
{% endhint %}

By default, OvenMediaEngine's API Server is disabled, so the following settings are required to use the API.

## Setup API Server

### Port Binding

The API server's port can be set in `<Bind><Managers><API>`. `<Port>` is an unsecured port and `<TLSPort>` is a secured port. To use TLSPort, TLS certificate must be set in the [Managers](./#managers).

```markup
<Server version="8">
	...
	<Bind>
		<Managers>
			<API>
				<Port>8081</Port>
				<TLSPort>8082</TLSPort>
			</API>
		</Managers>
		...
	</Bind>
	...
</Server>
```

### Managers

In order to use the API server, you must configure `<Managers>` as well as port binding.

```xml
<Server version="8">
	<Bind>
		...
	</Bind>

	<Managers>
		<Host>
			<Names>
				<Name>*</Name>
			</Names>
			<TLS>
				<CertPath>airensoft_com.crt</CertPath>
				<KeyPath>airensoft_com.key</KeyPath>
				<ChainCertPath>airensoft_com_chain.crt</ChainCertPath>
			</TLS>
		</Host>
		<API>
			<AccessToken>your_access_token</AccessToken>
			<CrossDomains>
				<Url>*.airensoft.com</Url>
				<Url>http://*.sub-domain.airensoft.com</Url>
				<Url>http?://airensoft.*</Url>
			</CrossDomains>
		</API>
	</Managers>

	<VirtualHosts>
		...
	</VirtualHosts>
</Server>
```

#### Host

In , set the domain or IP that can access the API server. If \* is set, any address is used. In order to access using the TLS Port, a certificate must be set in .

#### AccessToken

API Server uses Basic HTTP Authentication Scheme to authenticate clients. An `AccessToken` is a plaintext credential string before base64 encoding. Setting the AccessToken to the form `user-id:password` per RFC7617 allows standard browsers to pass authentication, but it is not required.

For more information about HTTP Basic authentication, refer to the URL below.&#x20;

[https://developer.mozilla.org/en-US/docs/Web/HTTP/Authentication](https://developer.mozilla.org/en-US/docs/Web/HTTP/Authentication)

#### CrossDomains

To enable CORS on your API Server, you can add a setting. You can add \* to allow all domains. If contains a scheme, such as https://, only that scheme can be allowed, or if the scheme is omitted, such as \*.airensoft.com, all schemes can be accepted.



