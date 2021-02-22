# REST API \(Beta\)

## Overview

The REST APIs provided by OME allow you to query or change settings such as VirtualHost and Application/Stream.

{% hint style="warning" %}
The APIs are currently beta version, so there are some limitations/considerations.

* Settings of VirtualHost can only be viewed and cannot be changed or deleted.
* If you add/change/delete the settings of the App/Output Profile by invoking the API, the app will be restarted. This means that all sessions associated with the app will be disconnected.
* The API version is fixed with v1 until the experimental stage is complete, and the detailed specification can be changed at any time.
{% endhint %}

By default, OvenMediaEngine's APIs are disabled, so the following settings are required to use the API:

## Setting up for using the APIs

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

## API Request/Response Specification

In this manual, the following format is used when describing the API.

{% api-method method="get" host="http://<OME\_HOST>:<API\_PORT>" path="/<VERSION>/<API\_PATH>\[/...\]" %}
{% api-method-summary %}
&lt;VERSION&gt;/&lt;API\_PATH&gt;
{% endapi-method-summary %}

{% api-method-description %}
Here is the description of the API  
  
Request Example:  
`- Method: GET  
- URL: http://1.2.3.4:8081/v1/vhost  
- Header:  
  authorization: Basic b21ldGVzdA==`
{% endapi-method-description %}

{% api-method-spec %}
{% api-method-request %}
{% api-method-path-parameters %}
{% api-method-parameter name="" type="string" required=false %}

{% endapi-method-parameter %}
{% endapi-method-path-parameters %}

{% api-method-headers %}
{% api-method-parameter name="authorization" type="string" required=false %}
`Basic base64encode(AccessToken)`  
  
if you set `<AccessToken>` to "ome-access-token" in `Server.xml`, you must set `Basic b21lLWFjY2Vzcy10b2tlbg==` in the `Authorization` header.
{% endapi-method-parameter %}
{% endapi-method-headers %}
{% endapi-method-request %}

{% api-method-response %}
{% api-method-response-example httpCode=200 %}
{% api-method-response-example-description %}

{% endapi-method-response-example-description %}

```

```
{% endapi-method-response-example %}
{% endapi-method-response %}
{% endapi-method-spec %}
{% endapi-method %}



#### &lt;OME\_HOST&gt;

This means the IP or domain of the server on which your OME is running.

#### &lt;API\_PORT&gt;

This means the port number of the API you set up in `Server.xml`. The default value is 8081.

#### &lt;VERSION&gt;

Indicates the version of the API. Currently, all APIs are v1.

#### &lt;API\_PATH&gt;

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





