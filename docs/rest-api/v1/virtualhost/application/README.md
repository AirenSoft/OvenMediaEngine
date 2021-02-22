# Application

{% api-method method="post" host="http://<OME\_HOST>:<API\_PORT>" path="/v1/vhosts/{vhost\_name}/apps" %}
{% api-method-summary %}
/v1/vhosts/{vhost\_name}/apps
{% endapi-method-summary %}

{% api-method-description %}
Creates `Application`s in the `VirtualHost`  
  
Request Example:  
`POST http://1.2.3.4:8081/v1/vhosts/default/apps  
  
[  
  {  
    "name": "app",  
    "type": "live",  
    "outputProfiles": [  
      {  
        "name": "bypass_profile",  
        "outputStreamName": "${OriginStreamName}",  
        "encodes": {  
          "videos": [  
            {  
              "bypass": true  
            }  
          ],  
          "audios": [  
            {  
              "bypass": true  
            }  
          ]  
        }  
      }  
    ]  
  }  
]`
{% endapi-method-description %}

{% api-method-spec %}
{% api-method-request %}
{% api-method-path-parameters %}
{% api-method-parameter required=true name="vhost\_name" type="string" %}
A name of `VirtualHost`
{% endapi-method-parameter %}
{% endapi-method-path-parameters %}

{% api-method-headers %}
{% api-method-parameter required=true name="authorization" type="string" %}
A string for authentication in `Basic Base64(AccessToken)` format.  
For example, `Basic b21lLWFjY2Vzcy10b2tlbg==` if access token is `ome-access-token`.
{% endapi-method-parameter %}
{% endapi-method-headers %}

{% api-method-body-parameters %}
{% api-method-parameter required=true name="\(json body\)" type="array" %}
A list of `Application`
{% endapi-method-parameter %}
{% endapi-method-body-parameters %}
{% endapi-method-request %}

{% api-method-response %}
{% api-method-response-example httpCode=200 %}
{% api-method-response-example-description %}
- Return type: List&lt;Response&lt;`Application`&gt;&gt;  
- Description  
Returns a list of created application informations
{% endapi-method-response-example-description %}

```
[
	{
		"statusCode": 200,
		"message": "OK",
		"response": {
			"dynamic": false,
			"name": "app",
			"outputProfiles": [
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
	"message": "Could not find the virtual host: [non-exists] (404)"
}
```
{% endapi-method-response-example %}
{% endapi-method-response %}
{% endapi-method-spec %}
{% endapi-method %}

{% api-method method="get" host="http://<OME\_HOST>:<API\_PORT>" path="/v1/vhosts/{vhost\_name}/apps" %}
{% api-method-summary %}
/v1/vhosts/{vhost\_name}/apps
{% endapi-method-summary %}

{% api-method-description %}
Lists all application names in the `VirtualHost`  
  
Request Example:  
`GET http://1.2.3.4:8081/v1/vhosts/default/apps`
{% endapi-method-description %}

{% api-method-spec %}
{% api-method-request %}
{% api-method-path-parameters %}
{% api-method-parameter required=true name="vhost\_name" type="string" %}
A name of VirtualHost
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
Returns a list of application names
{% endapi-method-response-example-description %}

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
	"message": "Could not find the virtual host: [non-exists] (404)"
}
```
{% endapi-method-response-example %}
{% endapi-method-response %}
{% endapi-method-spec %}
{% endapi-method %}

{% api-method method="get" host="http://<OME\_HOST>:<API\_PORT>" path="/v1/vhosts/{vhost\_name}/apps/{app\_name}" %}
{% api-method-summary %}
/v1/vhosts/{vhost\_name}/apps/{app\_name}
{% endapi-method-summary %}

{% api-method-description %}
Gets the configuration of the `Application`  
  
Request Example:  
`GET http://1.2.3.4:8081/v1/vhosts/default/apps/app`
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
- Return type: Response&lt;`Application`&gt;  
- Description  
Returns the specified application information
{% endapi-method-response-example-description %}

```
{
	"statusCode": 200,
	"message": "OK",
	"response": {
		"dynamic": false,
		"name": "app",
		"outputProfiles": [
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

{% api-method method="put" host="http://<OME\_HOST>:<API\_PORT>" path="/v1/vhosts/{vhost\_name}/apps/{app\_name}" %}
{% api-method-summary %}
/v1/vhosts/{vhost\_name}/apps/{app\_name}
{% endapi-method-summary %}

{% api-method-description %}
Changes the configuration of the `Application`  
  
Request Example:  
`PUT http://1.2.3.4:8081/v1/vhosts/default/apps/app  
  
{  
  "type": "live"  
}`
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

{% api-method-body-parameters %}
{% api-method-parameter required=true name="\(json body\)" type="object" %}
`Application`
{% endapi-method-parameter %}
{% endapi-method-body-parameters %}
{% endapi-method-request %}

{% api-method-response %}
{% api-method-response-example httpCode=200 %}
{% api-method-response-example-description %}
- Return type: Response&lt;`Application`&gt;  
- Description  
Returns the modified application information
{% endapi-method-response-example-description %}

```
{
	"statusCode": 200,
	"message": "OK",
	"response": {
		"dynamic": false,
		"name": "app",
		"outputProfiles": [
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

{% api-method method="delete" host="http://<OME\_HOST>:<API\_PORT>" path="/v1/vhosts/{vhost\_name}/apps/{app\_name}" %}
{% api-method-summary %}
/v1/vhosts/{vhost\_name}/apps/{app\_name}
{% endapi-method-summary %}

{% api-method-description %}
Deletes the `Application`  
  
Request Example:  
`DELETE http://1.2.3.4:8081/v1/vhosts/default/apps/app`
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
- Return type: Response&lt;&gt;  
- Description  
Returns the result
{% endapi-method-response-example-description %}

```
{
	"statusCode": 200,
	"message": "OK"
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

