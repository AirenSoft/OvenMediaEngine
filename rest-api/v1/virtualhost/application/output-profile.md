# Output Profile

{% api-method method="post" host="http://<OME\_HOST>:<API\_PORT>" path="/v1/vhosts/{vhost\_name}/apps/{app\_name}/outputProfiles" %}
{% api-method-summary %}
/v1/vhosts/{vhost\_name}/apps/{app\_name}/outputProfiles
{% endapi-method-summary %}

{% api-method-description %}
Creates `OutputProfile`s in the `Application`  
  
Request Example:  
`POST http://1.2.3.4:8081/v1/vhosts/default/apps/app/outputProfiles  
  
[  
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
]`
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
{% api-method-parameter required=true name="\(json body\)" type="array" %}
List&lt;`OutputProfile`&gt;
{% endapi-method-parameter %}
{% endapi-method-body-parameters %}
{% endapi-method-request %}

{% api-method-response %}
{% api-method-response-example httpCode=200 %}
{% api-method-response-example-description %}
- Return type: List&lt;Response&lt;`OutputProfile`&gt;&gt;  
- Description  
Returns a list of created output profile informations
{% endapi-method-response-example-description %}

```
[
	{
		"statusCode": 200,
		"message": "OK",
		"response": {
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
	"message": "Could not find the application: [default/non-exists] (404)"
}
```
{% endapi-method-response-example %}
{% endapi-method-response %}
{% endapi-method-spec %}
{% endapi-method %}

{% api-method method="get" host="http://<OME\_HOST>:<API\_PORT>" path="/v1/vhosts/{vhost\_name}/apps/{app\_name}/outputProfiles" %}
{% api-method-summary %}
/v1/vhosts/{vhost\_name}/apps/{app\_name}/outputProfiles
{% endapi-method-summary %}

{% api-method-description %}
Lists all output profile names in the `Application`  
  
Request Example:  
`GET http://1.2.3.4:8081/v1/vhosts/default/apps/app/outputProfiles`
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
Returns a list of output profile names
{% endapi-method-response-example-description %}

```
{
	"statusCode": 200,
	"message": "OK",
	"response": [
		"bypass.opus",
		"bypass_profile"
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

{% api-method method="get" host="http://<OME\_HOST>:<API\_PORT>" path="/v1/vhosts/{vhost\_name}/apps/{app\_name}/outputProfiles/{profile\_name}" %}
{% api-method-summary %}
/v1/vhosts/{vhost\_name}/apps/{app\_name}/outputProfiles/{profile\_name}
{% endapi-method-summary %}

{% api-method-description %}
Gets the configuration of the `OutputProfile`  
  
Request Example:  
`GET http://1.2.3.4:8081/v1/vhosts/default/apps/app/outputProfiles/bypass_profile`
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

{% api-method-parameter required=true name="profile\_name" type="string" %}
A name of `OutputProfile`
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
- Return type: Response&lt;`OutputProfile`&gt;  
- Description  
Returns the specified output profile information
{% endapi-method-response-example-description %}

```
{
	"statusCode": 200,
	"message": "OK",
	"response": {
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

{% api-method method="put" host="http://<OME\_HOST>:<API\_PORT>" path="/v1/vhosts/{vhost\_name}/apps/{app\_name}/outputProfiles/{profile\_name}" %}
{% api-method-summary %}
/v1/vhosts/{vhost\_name}/apps/{app\_name}/outputProfiles/{profile\_name}
{% endapi-method-summary %}

{% api-method-description %}
Changes the configuration of the `OutputProfile`  
  
Request Example:  
`PUT http://1.2.3.4:8081/v1/vhosts/default/apps/app/outputProfiles/bypass_profile  
  
{  
  "outputStreamName": "${OriginStreamName}",  
  "encodes": {  
    "videos": [  
      {  
        "codec": "h264",  
        "bitrate": "3M",  
        "width": 1280,  
        "height": 720,  
        "framerate": 30  
      }  
    ],  
    "audios": [  
      {  
        "bypass": true  
      }  
    ]  
  }  
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

{% api-method-parameter required=true name="profile\_name" type="string" %}
A name of `OutputProfile`
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
`OutputProfile`
{% endapi-method-parameter %}
{% endapi-method-body-parameters %}
{% endapi-method-request %}

{% api-method-response %}
{% api-method-response-example httpCode=200 %}
{% api-method-response-example-description %}
- Return type: Response&lt;`OutputProfile`&gt;  
- Description  
Returns the modified output profile information
{% endapi-method-response-example-description %}

```
{
	"statusCode": 200,
	"message": "OK",
	"response": {
		"encodes": {
			"audios": [
				{
					"bypass": true
				}
			],
			"images": [],
			"videos": [
				{
					"active": true,
					"bitrate": "3M",
					"codec": "h264",
					"framerate": 30.0,
					"height": 720,
					"width": 1280
				}
			]
		},
		"name": "bypass_profile",
		"outputStreamName": "${OriginStreamName}"
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

{% api-method method="delete" host="http://<OME\_HOST>:<API\_PORT>" path="/v1/vhosts/{vhost\_name}/apps/{app\_name}/outputProfiles/{profile\_name}" %}
{% api-method-summary %}
/v1/vhosts/{vhost\_name}/apps/{app\_name}/outputProfiles/{profile\_name}
{% endapi-method-summary %}

{% api-method-description %}
Deletes the `OutputProfile`  
  
Request Example:  
`DELETE http://1.2.3.4:8081/v1/vhosts/default/apps/app/outputProfiles/bypass_profile`
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

{% api-method-parameter required=true name="profile\_name" type="string" %}
A name of `OutputProfile`
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
	"message": "Could not find the output profile: [default/app/non-exists] (404)"
}
```
{% endapi-method-response-example %}
{% endapi-method-response %}
{% endapi-method-spec %}
{% endapi-method %}

