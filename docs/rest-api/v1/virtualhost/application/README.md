# Application

{% swagger baseUrl="http://<OME_HOST>:<API_PORT>" path="/v1/vhosts/{vhost_name}/apps" method="post" summary="/v1/vhosts/{vhost_name}/apps" %}
{% swagger-description %}
Creates 

`Application`

s in the 

`VirtualHost`

\




\


Request Example:

\




`POST http://1.2.3.4:8081/v1/vhosts/default/apps`

\


``

\[ { "name": "app", "type": "live", "outputProfiles": { "outputProfile": [ { "name": "bypass_profile", "outputStreamName": "${OriginStreamName}", "encodes": { "videos": [ { "bypass": true } ], "audios": [ { "bypass": true } ] } } ] } ]
{% endswagger-description %}

{% swagger-parameter in="path" name="vhost_name" type="string" %}
A name of 

`VirtualHost`
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

{% swagger-parameter in="body" name="(json body)" type="array" %}
A list of 

`Application`
{% endswagger-parameter %}

{% swagger-response status="200" description="- Return type: List<Response<Application>>
- Description
Returns a list of created application informations" %}
```
[
	{
		"statusCode": 200,
		"message": "OK",
		"response": {
			"dynamic": false,
			"name": "app",
			"outputProfiles":{
				"outputProfile": [
				{
					"encodes": {
						"audios": [
							{
								"bypass": true
							}
						],
						"images": [],
						"videos": [
							{
								"bypass": true
							}
						]
					},
					"name": "bypass_profile",
					"outputStreamName": "${OriginStreamName}"
				}
				],
			},
			"providers": {
				"mpegts": {},
				"rtmp": {}
			},
			"publishers": {
				"dash": {
					"segmentCount": 3,
					"segmentDuration": 5
				},
				"hls": {
					"segmentCount": 3,
					"segmentDuration": 5
				},
				"llDash": {
					"segmentDuration": 3
				},
				"sessionLoadBalancingThreadCount": 8,
				"webrtc": {
					"timeout": 30000
				}
			},
			"type": "live"
		}
	}
]
```
{% endswagger-response %}

{% swagger-response status="404" description="- Return type: Response<>
- Description
Not Found" %}
```
{
	"statusCode": 404,
	"message": "Could not find the virtual host: [non-exists] (404)"
}
```
{% endswagger-response %}
{% endswagger %}

{% swagger baseUrl="http://<OME_HOST>:<API_PORT>" path="/v1/vhosts/{vhost_name}/apps" method="get" summary="/v1/vhosts/{vhost_name}/apps" %}
{% swagger-description %}
Lists all application names in the 

`VirtualHost`

\




\


Request Example:

\




`GET http://1.2.3.4:8081/v1/vhosts/default/apps`
{% endswagger-description %}

{% swagger-parameter in="path" name="vhost_name" type="string" %}
A name of VirtualHost
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
Returns a list of application names" %}
```
{
	"statusCode": 200,
	"message": "OK",
	"response": [
		"app",
		"app2",
		"test_app"
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
	"message": "Could not find the virtual host: [non-exists] (404)"
}
```
{% endswagger-response %}
{% endswagger %}

{% swagger baseUrl="http://<OME_HOST>:<API_PORT>" path="/v1/vhosts/{vhost_name}/apps/{app_name}" method="get" summary="/v1/vhosts/{vhost_name}/apps/{app_name}" %}
{% swagger-description %}
Gets the configuration of the 

`Application`

\




\


Request Example:

\




`GET http://1.2.3.4:8081/v1/vhosts/default/apps/app`
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

{% swagger-response status="200" description="- Return type: Response<Application>
- Description
Returns the specified application information" %}
```
{
	"statusCode": 200,
	"message": "OK",
	"response": {
		"dynamic": false,
		"name": "app",
		"outputProfiles":{
			"outputProfile": [
			{
				"encodes": {
					"audios": [
						{
							"bypass": true
						},
						{
							"active": true,
							"bitrate": "128000",
							"channel": 2,
							"codec": "opus",
							"samplerate": 48000
						}
					],
					"images": [],
					"videos": [
						{
							"bypass": true
						}
					]
				},
				"name": "bypass.opus",
				"outputStreamName": "${OriginStreamName}"
			}
			],
		},
		"providers": {
			"mpegts": {},
			"ovt": {},
			"rtmp": {},
			"rtspPull": {}
		},
		"publishers": {
			"dash": {
				"crossDomains": [
					"*"
				],
				"segmentCount": 3,
				"segmentDuration": 5
			},
			"file": {
				"fileInfoPath": "/root/temp/ome_rec/${Application}/${Stream}/${Id}_${Sequence}.xml",
				"filePath": "/root/temp/ome_rec/${Application}/${Stream}/${Id}_${Sequence}.mp4"
			},
			"hls": {
				"crossDomains": [
					"*"
				],
				"segmentCount": 3,
				"segmentDuration": 5
			},
			"llDash": {
				"crossDomains": [
					"*"
				],
				"segmentDuration": 5
			},
			"ovt": {},
			"sessionLoadBalancingThreadCount": 8,
			"webrtc": {
				"timeout": 30000
			}
		},
		"type": "live"
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
	"message": "Could not find the application: [default/non-exists] (404)"
}
```
{% endswagger-response %}
{% endswagger %}

{% swagger baseUrl="http://<OME_HOST>:<API_PORT>" path="/v1/vhosts/{vhost_name}/apps/{app_name}" method="put" summary="/v1/vhosts/{vhost_name}/apps/{app_name}" %}
{% swagger-description %}
Changes the configuration of the 

`Application`

\




\


Request Example:

\




`PUT http://1.2.3.4:8081/v1/vhosts/default/apps/app`

\


``

\


`{`

\


`   "type": "live" `

\


`}`
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

{% swagger-parameter in="body" name="(json body)" type="object" %}
`Application`
{% endswagger-parameter %}

{% swagger-response status="200" description="- Return type: Response<Application>
- Description
Returns the modified application information" %}
```
{
	"statusCode": 200,
	"message": "OK",
	"response": {
		"dynamic": false,
		"name": "app",
		"outputProfiles": {
			"outputProfile": [
			{
				"encodes": {
					"audios": [
						{
							"bypass": true
						}
					],
					"images": [],
					"videos": [
						{
							"bypass": true
						}
					]
				},
				"name": "bypass_profile",
				"outputStreamName": "${OriginStreamName}"
			}
			],
		},
		"providers": {
			"mpegts": {},
			"rtmp": {}
		},
		"publishers": {
			"dash": {
				"segmentCount": 3,
				"segmentDuration": 5
			},
			"hls": {
				"segmentCount": 3,
				"segmentDuration": 5
			},
			"llDash": {
				"segmentDuration": 3
			},
			"sessionLoadBalancingThreadCount": 8,
			"webrtc": {
				"timeout": 30000
			}
		},
		"type": "live"
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
	"message": "Could not find the application: [default/non-exists] (404)"
}
```
{% endswagger-response %}
{% endswagger %}

{% swagger baseUrl="http://<OME_HOST>:<API_PORT>" path="/v1/vhosts/{vhost_name}/apps/{app_name}" method="delete" summary="/v1/vhosts/{vhost_name}/apps/{app_name}" %}
{% swagger-description %}
Deletes the 

`Application`

\




\


Request Example:

\




`DELETE http://1.2.3.4:8081/v1/vhosts/default/apps/app`
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

{% swagger-response status="200" description="- Return type: Response<>
- Description
Returns the result" %}
```
{
	"statusCode": 200,
	"message": "OK"
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
