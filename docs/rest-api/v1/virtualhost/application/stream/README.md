# Stream

## Get Stream List

> #### <mark style="color:blue;">**GET**</mark> /v1/vhosts/{vhost name}/apps/{app name}/streams

Get all stream names in the {vhost name}/{app name} application.

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
	"response": [
		"stream",
		"stream2"
	]
}
```

_statusCode_

  Same as HTTP Status Code

_message_

  A human-readable description of the response code

_response_

  Json array containing a list of stream names

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

---

## Create Stream (Pull)

> #### <mark style="color:blue;">**GET**</mark> /v1/vhosts/{vhost name}/apps/{app name}/streams

Create a stream by pulling an external URL. External URL protocols currently support RTSP and OVT.

### **Header**

```
Authorization: Basic {credentials}
Content-Type: application/json
```

_Authorization_

  Credentials for HTTP Basic Authentication created with \<AccessToken>

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
```
_name_ (required)

  Stream name to create

_urls_ (required)

  A list of URLs to pull streams from, in Json array format. All URLs must have the same scheme.

_properties_ (optional)

  _persistent_

    Created as a persistent stream, not deleted until DELETE

  _noInputFailoverTimeoutMs_

    If no data is input during this period, the stream is deleted, but ignored if persistent is true

  _unusedStreamDeletionTimeoutMs_

    If no data is output during this period (if there is no viewer), the stream is deleted, but ignored if persistent is true

### Responses

<details>

<summary>201 Created</summary>

A stream has been created.

**Header**

```
Content-Type: application/json
```

**Body**

```json
{
    "message": "Created",
    "statusCode": 201
}
```

_statusCode_

 Same as HTTP Status Code

_message_

 A human-readable description of the response code

</details>

<details>

<summary>400 Bad Request</summary>

Invalid request. Body is not a Json Object or does not have a required value

</details>

<details>

<summary>401 Unauthorized</summary>

Authentication required

**Header**

```json
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

---

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
```

_statusCode_

 Same as HTTP Status Code

_message_

 A human-readable description of the response code

_response_

 Details of the stream

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
