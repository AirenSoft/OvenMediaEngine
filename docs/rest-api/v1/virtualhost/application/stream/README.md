# Stream

## Get Stream List

> #### <mark style="color:blue;">**GET**</mark> /v1/vhosts/{vhost name}/apps/{app name}/streams

Get all stream names in the {vhost name}/{app name} application.

### **Header**

```http
Authorization: Basic {credentials}

# Authorization
    Credentials for HTTP Basic Authentication created with <AccessToken>
```

### Responses

<details>

<summary>200 Ok</summary>

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

<summary>404 Not Found</summary>

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

***

## Create Stream (Pull)

> #### <mark style="color:blue;">**POST**</mark> /v1/vhosts/{vhost name}/apps/{app name}/streams

Create a stream by pulling an external URL. External URL protocols currently support RTSP and OVT.

### **Header**

```http
Authorization: Basic {credentials}
Content-Type: application/json

# Authorization
    Credentials for HTTP Basic Authentication created with <AccessToken>
```

### **Body**

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
```

### Responses

<details>

<summary>201 Created</summary>

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

<summary>400 Bad Request</summary>

Invalid request. Body is not a Json Object or does not have a required value

</details>

<details>

<summary>401 Unauthorized</summary>

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

<summary>404 Not Found</summary>

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

<summary>409 Conflict</summary>

A stream with the same name already exists

</details>

<details>

<summary>502 Bad Gateway</summary>

Failed to pull provided URL

</details>

<details>

<summary>500 Internal Server Error</summary>

Unknown error

</details>

## Get Stream Info

> #### <mark style="color:blue;">**GET**</mark> /v1/vhosts/{vhost name}/apps/{app name}/streams/{stream name}

Get detailed information of {vhost name}/{app name}/{stream name} stream.

### **Header**

```
Authorization: Basic {credentials}
```

_Authorization_

Credentials for HTTP Basic Authentication created with \<AccessToken>

### Responses

<details>

<summary>200 Ok</summary>

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

</details>

<details>

<summary>404 Not Found</summary>

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
