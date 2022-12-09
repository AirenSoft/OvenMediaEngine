# Stream

{% swagger baseUrl="http://<OME_HOST>:<API_PORT>" path="/v1/vhosts/{vhost_name}/apps/{app_name}/streams" method="get" summary="/v1/vhosts/{vhost_name}/apps/{app_name}/streams" %}
{% swagger-description %}
Lists all stream names in the 

`Application`

\




\


Request Example:

\




`GET http://1.2.3.4:8081/v1/vhosts/default/apps/app/streams`
{% endswagger-description %}

{% swagger-parameter in="path" name="vhost_name" type="string" %}
A name of 

`VirtualHost`
{% endswagger-parameter %}

{% swagger-parameter in="path" name="app_name" type="string" %}
A name of 

`Application`
{% endswagger-parameter %}

{% swagger-parameter in="header" name="authorization" type="string" %}
A string for authentication in 

`Basic Base64(AccessToken)`

 format.

\


For example, 

`Basic b21lLWFjY2Vzcy10b2tlbg==`

 if access token is 

`ome-access-token`

.
{% endswagger-parameter %}

{% swagger-response status="200" description="- Return type: Response<List<string>>
- Description
Returns a list of stream names" %}
```
{
	"statusCode": 200,
	"message": "OK",
	"response": [
		"stream"
	]
}
```
{% endswagger-response %}

{% swagger-response status="404" description="- Return type: Response<>
- Description
Not Found" %}
```
{
	"statusCode": 404,
	"message": "Could not find the application: [default/non-exists] (404)"
}
```
{% endswagger-response %}
{% endswagger %}

{% swagger baseUrl="http://<OME_HOST>:<API_PORT>" path="/v1/vhosts/{vhost_name}/apps/{app_name}/streams/{stream_name}" method="get" summary="/v1/vhosts/{vhost_name}/apps/{app_name}/streams/{stream_name}" %}
{% swagger-description %}
Gets the configuration of the 

`Stream`

\




\


Request Example:

\




`GET http://1.2.3.4:8081/v1/vhosts/default/apps/app/streams/stream`
{% endswagger-description %}

{% swagger-parameter in="path" name="vhost_name" type="string" %}
A name of 

`VirtualHost`
{% endswagger-parameter %}

{% swagger-parameter in="path" name="app_name" type="string" %}
A name of 

`Application`
{% endswagger-parameter %}

{% swagger-parameter in="path" name="stream_name" type="string" %}
A name of 

`Stream`
{% endswagger-parameter %}

{% swagger-parameter in="header" name="authorization" type="string" %}
A string for authentication in 

`Basic Base64(AccessToken)`

 format.

\


For example, 

`Basic b21lLWFjY2Vzcy10b2tlbg==`

 if access token is 

`ome-access-token`

.
{% endswagger-parameter %}

{% swagger-response status="200" description="- Return type: Response<Stream>
- Description
Returns the specified stream information" %}
```
{
	"statusCode": 200,
	"message": "OK",
	"response": {
		"input": {
			"createdTime": "2021-01-11T03:45:21.879+09:00",
			"sourceType": "Rtmp",
			"tracks": [
				{
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
						"type": "Video",
						"video": {
							"bypass": true
						}
					},
					{
						"audio": {
							"bypass": true
						},
						"type": "Audio"
					},
					{
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
{% endswagger-response %}

{% swagger-response status="404" description="- Return type: Response<>
- Description
Not Found" %}
```
{
	"statusCode": 404,
	"message": "Could not find the output profile: [default/app/non-exists] (404)"
}
```
{% endswagger-response %}
{% endswagger %}
