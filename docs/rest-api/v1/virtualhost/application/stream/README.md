# Stream

## Get Stream List

Get all stream names in the {vhost name}/{app name} application.

> #### Request

<details>

<summary><mark style="color:blue;">GET</mark> /v1/vhosts/{vhost}/apps/{app}/streams</summary>

**Header**

```http
Authorization: Basic {credentials}

# Authorization
    Credentials for HTTP Basic Authentication created with <AccessToken>
```

</details>

> #### Responses

<details>

<summary><mark style="color:blue;">200</mark> Ok</summary>

The request has succeeded

**Header**

```
Content-Type: application/json
```

**Body**

```json
{
	"statusCode": 200,
	"message": "OK",
	"response": [
		"stream",
		"stream2"
	]
}

# statusCode
	Same as HTTP Status Code
# message
	A human-readable description of the response code
# response
	Json array containing a list of stream names
```

</details>

<details>

<summary><mark style="color:red;">401</mark> Unauthorized</summary>

Authentication required

**Header**

```http
WWW-Authenticate: Basic realm=”OvenMediaEngine”
```

**Body**

```json
{
    "message": "[HTTP] Authorization header is required to call API (401)",
    "statusCode": 401
}
```

</details>

<details>

<summary><mark style="color:red;">404</mark> Not Found</summary>

The given vhost name or app name could not be found.

**Header**

```json
Content-Type: application/json
```

**Body**

```json
{
    "statusCode": 404,
    "message": "Could not find the application: [default/non-exists] (404)"
}
```

</details>

## Create Stream (Pull)

Create a stream by pulling an external URL. External URL protocols currently support RTSP and OVT.

> #### Request

<details>

<summary><mark style="color:blue;">POST</mark> /v1/vhosts/{vhost}/apps/{app}/streams</summary>

**Header**

```http
Authorization: Basic {credentials}
Content-Type: application/json

# Authorization
    Credentials for HTTP Basic Authentication created with <AccessToken>
```

**Body**

```json
{
	"name": "new_stream_name",
	"urls": [
		"rtsp://192.168.0.160:553/app/stream",
		"url to pull the stream from - support OVT/RTSP",
		"Only urls with the same scheme can be sent as a group."
  	],
  	"properties":{
		"persistent": false,
		"noInputFailoverTimeoutMs": 3000,
		"unusedStreamDeletionTimeoutMs": 60000,
		"ignoreRtcpSRTimestamp": false
  	}
}

# name (required)
	Stream name to create
# urls (required)
	A list of URLs to pull streams from, in Json array format. 
	All URLs must have the same scheme.
# properties (optional)
	## persistent
		Created as a persistent stream, not deleted until DELETE
	## noInputFailoverTimeoutMs
		If no data is input during this period, the stream is deleted, 
		but ignored if persistent is true
	## unusedStreamDeletionTimeoutMs
		If no data is output during this period (if there is no viewer), 
		the stream is deleted, but ignored if persistent is true
	## ignoreRtcpSRTimestamp
		No waits RTCP SR and start stream immediately
```

</details>

> #### Responses

<details>

<summary><mark style="color:blue;">201</mark> Created</summary>

A stream has been created.

**Header**

```http
Content-Type: application/json
```

**Body**

```json
{
    "message": "Created",
    "statusCode": 201
}

# statusCode
    Same as HTTP Status Code
# message
    A human-readable description of the response code
```

</details>

<details>

<summary><mark style="color:red;">400</mark> Bad Request</summary>

Invalid request. Body is not a Json Object or does not have a required value

</details>

<details>

<summary><mark style="color:red;">401</mark> Unauthorized</summary>

Authentication required

**Header**

```http
WWW-Authenticate: Basic realm=”OvenMediaEngine”
```

**Body**

```json
{
    "message": "[HTTP] Authorization header is required to call API (401)",
    "statusCode": 401
}
```

</details>

<details>

<summary><mark style="color:red;">404</mark> Not Found</summary>

The given vhost name or app name could not be found.

**Body**

```json
{
    "statusCode": 404,
    "message": "Could not find the application: [default/non-exists] (404)"
}
```

</details>

<details>

<summary><mark style="color:red;">409</mark> Conflict</summary>

A stream with the same name already exists

</details>

<details>

<summary><mark style="color:red;">502</mark> Bad Gateway</summary>

Failed to pull provided URL

</details>

<details>

<summary><mark style="color:red;">500</mark> Internal Server Error</summary>

Unknown error

</details>

## Get Stream Info

Get detailed information of stream.

> #### Request

<details>

<summary><mark style="color:blue;">GET</mark> /v1/vhosts/{vhost}/apps/{app}/streams/{stream}</summary>

**Header**

```http
Authorization: Basic {credentials}

# Authorization
    Credentials for HTTP Basic Authentication created with <AccessToken>
```

</details>

> #### Responses

<details>

<summary><mark style="color:blue;">200</mark> Ok</summary>

The request has succeeded

**Header**

```
Content-Type: application/json
```

**Body**

```json
{
	"statusCode": 200,
	"message": "OK",
	"response": {
		"input": {
			"createdTime": "2021-01-11T03:45:21.879+09:00",
			"sourceType": "Rtmp",
			"tracks": [
			{
				"id": 0,
				"type": "Video",
				"video": {
					"bitrate": "2500000",
					"bypass": false,
					"codec": "H264",
					"framerate": 30.0,
					"hasBframes": false,
					"keyFrameInterval": 30,
					"height": 720,
					"width": 1280
				}
			},
			{
				"id": 1,				
				"audio": {
					"bitrate": "128000",
					"bypass": false,
					"channel": 2,
					"codec": "AAC",
					"samplerate": 48000
				},
				"type": "Audio"
			}
			]
		},
		"name": "stream",
		"outputs": [
		{
			"name": "stream",
			"tracks": [
			{
				"id": 0,
				"type": "Video",
				"video": {
					"bypass": true
				}
			},
			{
				"id": 1,					
				"audio": {
					"bypass": true
				},
				"type": "Audio"
			},
			{
				"id": 2,					
				"audio": {
					"bitrate": "128000",
					"bypass": false,
					"channel": 2,
					"codec": "OPUS",
					"samplerate": 48000
				},
				"type": "Audio"
			}
			]
		}
	]
	}
}


# statusCode
	Same as HTTP Status Code
# message
	A human-readable description of the response code
# response
	Details of the stream
```

{% code title="Note" overflow="wrap" lineNumbers="true" %}
```
keyFrameInterval is GOP size
```
{% endcode %}

</details>

<details>

<summary><mark style="color:red;">401</mark> Unauthorized</summary>

Authentication required

**Header**

```http
WWW-Authenticate: Basic realm=”OvenMediaEngine”
```

**Body**

```json
{
    "message": "[HTTP] Authorization header is required to call API (401)",
    "statusCode": 401
}
```

</details>

<details>

<summary><mark style="color:red;">404</mark> Not Found</summary>

The given vhost name or app name could not be found.

**Header**

```json
Content-Type: application/json
```

**Body**

```json
{
    "statusCode": 404,
    "message": "Could not find the application or stream (404)"
}
```

</details>

## Delete Stream

Delete Stream. This terminates the ingress connection.

{% hint style="warning" %}
The sender can reconnect after the connection is terminated. To prevent reconnection, you must use [AccessControl](../../../../../access-control/).
{% endhint %}

> #### Request

<details>

<summary><mark style="color:blue;">DELETE</mark> /v1/vhosts/{vhost}/apps/{app}/streams/{stream}</summary>

**Header**

```http
Authorization: Basic {credentials}

# Authorization
    Credentials for HTTP Basic Authentication created with <AccessToken>
```

</details>

> #### Responses

<details>

<summary><mark style="color:blue;">200</mark> Ok</summary>

The request has succeeded

**Header**

```
Content-Type: application/json
```

**Body**

```json
{
	"statusCode": 200,
	"message": "OK",
}


# statusCode
	Same as HTTP Status Code
# message
	A human-readable description of the response code
```

</details>

<details>

<summary><mark style="color:red;">401</mark> Unauthorized</summary>

Authentication required

**Header**

```http
WWW-Authenticate: Basic realm=”OvenMediaEngine”
```

**Body**

```json
{
    "message": "[HTTP] Authorization header is required to call API (401)",
    "statusCode": 401
}
```

</details>

<details>

<summary><mark style="color:red;">404</mark> Not Found</summary>

The given vhost name or app name could not be found.

**Header**

```json
Content-Type: application/json
```

**Body**

```json
{
    "message": "[HTTP] Could not find the stream: [default/#default#app/stream] (404)",
    "statusCode": 404
}
```

</details>
