# Stream

{% api-method method="get" host="http://<OME\_HOST>:<API\_PORT>" path="/v1/vhosts/{vhost\_name}/apps/{app\_name}/streams" %}
{% api-method-summary %}
/v1/vhosts/{vhost\_name}/apps/{app\_name}/streams
{% endapi-method-summary %}

{% api-method-description %}
Lists all stream names in the `Application`  
  
Request Example:  
`GET http://1.2.3.4:8081/v1/vhosts/default/apps/app/streams`
{% endapi-method-description %}

{% api-method-spec %}
{% api-method-request %}
{% api-method-path-parameters %}
{% api-method-parameter required=true name="vhost\_name" type="string" %}
A name of `VirtualHost`
{% endapi-method-parameter %}

{% api-method-parameter required=true name="app\_name" type="string" %}
A name of `Application`
{% endapi-method-parameter %}
{% endapi-method-path-parameters %}

{% api-method-headers %}
{% api-method-parameter required=true name="authorization" type="string" %}
A string for authentication in `Basic Base64(AccessToken)` format.  
For example, `Basic b21lLWFjY2Vzcy10b2tlbg==` if access token is `ome-access-token`.
{% endapi-method-parameter %}
{% endapi-method-headers %}
{% endapi-method-request %}

{% api-method-response %}
{% api-method-response-example httpCode=200 %}
{% api-method-response-example-description %}
- Return type: Response&lt;List&lt;string&gt;&gt;  
- Description  
Returns a list of stream names
{% endapi-method-response-example-description %}

```
{
	"statusCode": 200,
	"message": "OK",
	"response": [
		"stream"
	]
}
```
{% endapi-method-response-example %}

{% api-method-response-example httpCode=404 %}
{% api-method-response-example-description %}
- Return type: Response&lt;&gt;  
- Description  
Not Found
{% endapi-method-response-example-description %}

```
{
	"statusCode": 404,
	"message": "Could not find the application: [default/non-exists] (404)"
}
```
{% endapi-method-response-example %}
{% endapi-method-response %}
{% endapi-method-spec %}
{% endapi-method %}

{% api-method method="get" host="http://<OME\_HOST>:<API\_PORT>" path="/v1/vhosts/{vhost\_name}/apps/{app\_name}/streams/{stream\_name}" %}
{% api-method-summary %}
/v1/vhosts/{vhost\_name}/apps/{app\_name}/streams/{stream\_name}
{% endapi-method-summary %}

{% api-method-description %}
Gets the configuration of the `Stream`  
  
Request Example:  
`GET http://1.2.3.4:8081/v1/vhosts/default/apps/app/streams/stream`
{% endapi-method-description %}

{% api-method-spec %}
{% api-method-request %}
{% api-method-path-parameters %}
{% api-method-parameter required=true name="vhost\_name" type="string" %}
A name of `VirtualHost`
{% endapi-method-parameter %}

{% api-method-parameter required=true name="app\_name" type="string" %}
A name of `Application`
{% endapi-method-parameter %}

{% api-method-parameter required=true name="stream\_name" type="string" %}
A name of `Stream`
{% endapi-method-parameter %}
{% endapi-method-path-parameters %}

{% api-method-headers %}
{% api-method-parameter required=true name="authorization" type="string" %}
A string for authentication in `Basic Base64(AccessToken)` format.  
For example, `Basic b21lLWFjY2Vzcy10b2tlbg==` if access token is `ome-access-token`.
{% endapi-method-parameter %}
{% endapi-method-headers %}
{% endapi-method-request %}

{% api-method-response %}
{% api-method-response-example httpCode=200 %}
{% api-method-response-example-description %}
- Return type: Response&lt;`Stream`&gt;  
- Description  
Returns the specified stream information
{% endapi-method-response-example-description %}

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
{% endapi-method-response-example %}

{% api-method-response-example httpCode=404 %}
{% api-method-response-example-description %}
- Return type: Response&lt;&gt;  
- Description  
Not Found
{% endapi-method-response-example-description %}

```
{
	"statusCode": 404,
	"message": "Could not find the output profile: [default/app/non-exists] (404)"
}
```
{% endapi-method-response-example %}
{% endapi-method-response %}
{% endapi-method-spec %}
{% endapi-method %}

