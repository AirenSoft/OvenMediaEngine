# REST API (Beta)

## Overview

The REST APIs provided by OME allow you to query or change settings such as VirtualHost and Application/Stream.

{% hint style="warning" %}
The APIs are currently beta version, so there are some limitations/considerations.

* Settings of VirtualHost can only be viewed and cannot be changed or deleted.
* If you add/change/delete the settings of the App/Output Profile by invoking the API, the app will be restarted. This means that all sessions associated with the app will be disconnected.
* The API version is fixed with v1 until the experimental stage is complete, and the detailed specification can be changed at any time.
{% endhint %}

By default, OvenMediaEngine's APIs are disabled, so the following settings are required to use the API:

Setting up for using the APIs


### Port

Set the `<Port>` to use by the API server. If you omit `<Port>`, you will use the API server's default port, port `8081`.

```markup
<Server version="8">
	...
	<Bind>
		<Managers>
			<API>
				<Port>8081</Port>
				<!-- If you need TLS support, please uncomment below:
				<TLSPort>8082</TLSPort>
				-->
			</API>
		</Managers>
		...
	</Bind>
	...
</Server>
```

### Host and Permissions

`<Host>` sets the Host name and TLS certificate information to be used by the API server, and `<AccessToken>` sets the token to be used for authentication when calling the APIs. You must use this token to invoke the API of OvenMediaEngine.

```markup
<Server version="8">
	...
	<Managers>
		<Host>
			<Names>
				<Name>*</Name>
			</Names>
			<!--
				If you want to set up TLS, set it up by referring to the following:
			<TLS>
				<CertPath>airensoft_com.crt</CertPath>
				<KeyPath>airensoft_com.key</KeyPath>
				<ChainCertPath>airensoft_com_chain.crt</ChainCertPath>
			</TLS>
			-->
		</Host>
		<API>
			<AccessToken>your_access_token</AccessToken>
		</API>
	</Managers>
</Server>
```

### CrossDomains

If you face a CORS problem by calling the OME API on your browser, you can set `<CrossDomains>` as follows:

```markup
<Server version="10">
	...
	<Managers>
		<Host>
			<Names>
				<Name>*</Name>
			</Names>

		</Host>
		<API>
		...
			<CrossDomains>
				<Url>*.airensoft.com</Url>
				<Url>sub-domain.airensoft.com</Url>
				<Url>http://http-only.airensoft.com</Url>
			</CrossDomains>
		</API>
	</Managers>
</Server>
```

If protocol is omitted like `*.airensoft.com`, both HTTP and HTTPS are supported.

## API Request/Response Specification

In this manual, the following format is used when describing the API.

{% swagger baseUrl="http://<OME_HOST>:<API_PORT>" path="/<VERSION>/<API_PATH>[/...]" method="get" summary="<VERSION>/<API_PATH>" %}
{% swagger-description %}
Here is the description of the API

\




\


Request Example:

\




`- Method: GET`

\


`- URL: http://1.2.3.4:8081/v1/vhost`

\


`- Header:`

\


  

`authorization: Basic b21ldGVzdA==`
{% endswagger-description %}

{% swagger-parameter in="path" name="" type="string" %}

{% endswagger-parameter %}

{% swagger-parameter in="header" name="authorization" type="string" %}
`Basic base64encode(AccessToken)`

\




\


if you set 

`<AccessToken>`

 to "ome-access-token" in 

`Server.xml`

, you must set 

`Basic b21lLWFjY2Vzcy10b2tlbg==`

 in the 

`Authorization`

 header.
{% endswagger-parameter %}

{% swagger-response status="200" description="" %}
```
```
{% endswagger-response %}
{% endswagger %}



#### \<OME\_HOST>

This means the IP or domain of the server on which your OME is running.

#### \<API\_PORT>

This means the port number of the API you set up in `Server.xml`. The default value is 8081.

#### \<VERSION>

Indicates the version of the API. Currently, all APIs are v1.

#### \<API\_PATH>&#xD;

Indicates the API path to be called. The API path is usually in the following form:

```markup
/resource-group[/resource[/resource-group[/resource/...]]][:action]
```

`resource` means an item, such as `VirtualHost` or `Application`, and `action` is used to command an action to a specific resource, such as `push` or `record`.

### Response

All response results are provided in the HTTP status code and response body, and if there are multiple response results in the response, the HTTP status code will be `207 MultiStatus`. The API response data is in the form of an array of [`Response`](v1/data-types/classes.md#responsetype) or [`Response`](v1/data-types/classes.md#responsetype) as follows:



```javascript
// Single data request example

// << Request >>
// Request URI: GET /v1/vhosts/default
// Header:
//   authorization: Basic b21lLWFjY2Vzcy10b2tlbg==

// << Response >>
// HTTP Status code: 200 OK
// Response Body:
{
	"statusCode": 200,
	"message": "OK",
	"response": ... // Requested data
}
```

```javascript
// Multiple data request (Status codes are the same)

// << Request >>
// Request URI: POST /v1/vhost/default/apps
// Header:
//   authorization: Basic b21lLWFjY2Vzcy10b2tlbg==
// Request Body:
[
	{ ... }, // App information to create
	{ ... }, // App information to create
]

// << Response >>
// HTTP Status code: 200 OK
// Response Body:
[
	{
		"statusCode": 200,
		"message": "OK",
		"response": ... // App1
	},
	{
		"statusCode": 200,
		"message": "OK",
		"response": ... // App2
	}
]
```

```javascript
// Multiple data request (Different status codes)

// << Request >>
// Request URI: POST /v1/vhost/default/apps
// Header:
//   authorization: Basic b21lLWFjY2Vzcy10b2tlbg==
// Request Body:
[
	{ ... }, // App information to create
	{ ... }, // App information to create
]

// << Response >>
// HTTP Status code: 207 MultiStatus
// Response Body:
[
	{
		"statusCode": 200,
		"message": "OK",
		"response": ... // App1
	},
	{
		"statusCode": 404,
		"message": "Not found"
	}
]
```

## API Limitations

&#x20;`VirtualHost` settings created by `Server.xml` cannot be modified through API. This rule also applies to `Application`/`OutputStream`, etc. within that VirtualHost. So, if you call a POST/PUT/DELETE API for `VirtualHost`/`Application`/`OutputProfile` declared in `Server.xml`, it will not work with a `403 Forbidden`  error.

