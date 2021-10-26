# Output Profile

{% swagger baseUrl="http://<OME_HOST>:<API_PORT>" path="/v1/vhosts/{vhost_name}/apps/{app_name}/outputProfiles" method="post" summary="/v1/vhosts/{vhost_name}/apps/{app_name}/outputProfiles" %}
{% swagger-description %}
Creates 

`OutputProfile`

s in the 

`Application`

\




\


Request Example:

\




`POST http://1.2.3.4:8081/v1/vhosts/default/apps/app/outputProfiles`

\


``

\


`[`

\


`   { `

\


`     "name": "bypass_profile", `

\


`     "outputStreamName": "${OriginStreamName}", `

\


`     "encodes": { `

\


`       "videos": [ `

\


`         { `

\


`           "bypass": true `

\


`         } `

\


`       ], `

\


`       "audios": [ `

\


`         { `

\


`           "bypass": true `

\


`         } `

\


`       ] `

\


`     } `

\


`   } `

\


`]`
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

{% swagger-parameter in="body" name="(json body)" type="array" %}
List<

`OutputProfile`

\>
{% endswagger-parameter %}

{% swagger-response status="200" description="- Return type: List<Response<OutputProfile>>
- Description
Returns a list of created output profile informations" %}
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

{% swagger baseUrl="http://<OME_HOST>:<API_PORT>" path="/v1/vhosts/{vhost_name}/apps/{app_name}/outputProfiles" method="get" summary="/v1/vhosts/{vhost_name}/apps/{app_name}/outputProfiles" %}
{% swagger-description %}
Lists all output profile names in the 

`Application`

\




\


Request Example:

\




`GET http://1.2.3.4:8081/v1/vhosts/default/apps/app/outputProfiles`
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
Returns a list of output profile names" %}
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

{% swagger baseUrl="http://<OME_HOST>:<API_PORT>" path="/v1/vhosts/{vhost_name}/apps/{app_name}/outputProfiles/{profile_name}" method="get" summary="/v1/vhosts/{vhost_name}/apps/{app_name}/outputProfiles/{profile_name}" %}
{% swagger-description %}
Gets the configuration of the 

`OutputProfile`

\




\


Request Example:

\




`GET http://1.2.3.4:8081/v1/vhosts/default/apps/app/outputProfiles/bypass_profile`
{% endswagger-description %}

{% swagger-parameter in="path" name="vhost_name" type="string" %}
A name of 

`VirtualHost`
{% endswagger-parameter %}

{% swagger-parameter in="path" name="app_name" type="string" %}
A name of 

`Application`
{% endswagger-parameter %}

{% swagger-parameter in="path" name="profile_name" type="string" %}
A name of 

`OutputProfile`
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

{% swagger-response status="200" description="- Return type: Response<OutputProfile>
- Description
Returns the specified output profile information" %}
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

{% swagger baseUrl="http://<OME_HOST>:<API_PORT>" path="/v1/vhosts/{vhost_name}/apps/{app_name}/outputProfiles/{profile_name}" method="put" summary="/v1/vhosts/{vhost_name}/apps/{app_name}/outputProfiles/{profile_name}" %}
{% swagger-description %}
Changes the configuration of the 

`OutputProfile`

\




\


Request Example:

\




`PUT http://1.2.3.4:8081/v1/vhosts/default/apps/app/outputProfiles/bypass_profile`

\


``

\


`{`

\


`   "outputStreamName": "${OriginStreamName}", `

\


`   "encodes": { `

\


`     "videos": [ `

\


`       { `

\


`         "codec": "h264", `

\


`         "bitrate": "3M", `

\


`         "width": 1280, `

\


`         "height": 720, `

\


`         "framerate": 30 `

\


`       } `

\


`     ], `

\


`     "audios": [ `

\


`       { `

\


`         "bypass": true `

\


`       } `

\


`     ] `

\


`   } `

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

{% swagger-parameter in="path" name="profile_name" type="string" %}
A name of 

`OutputProfile`
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
`OutputProfile`
{% endswagger-parameter %}

{% swagger-response status="200" description="- Return type: Response<OutputProfile>
- Description
Returns the modified output profile information" %}
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

{% swagger baseUrl="http://<OME_HOST>:<API_PORT>" path="/v1/vhosts/{vhost_name}/apps/{app_name}/outputProfiles/{profile_name}" method="delete" summary="/v1/vhosts/{vhost_name}/apps/{app_name}/outputProfiles/{profile_name}" %}
{% swagger-description %}
Deletes the 

`OutputProfile`

\




\


Request Example:

\




`DELETE http://1.2.3.4:8081/v1/vhosts/default/apps/app/outputProfiles/bypass_profile`
{% endswagger-description %}

{% swagger-parameter in="path" name="vhost_name" type="string" %}
A name of 

`VirtualHost`
{% endswagger-parameter %}

{% swagger-parameter in="path" name="app_name" type="string" %}
A name of 

`Application`
{% endswagger-parameter %}

{% swagger-parameter in="path" name="profile_name" type="string" %}
A name of 

`OutputProfile`
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
	"message": "Could not find the output profile: [default/app/non-exists] (404)"
}
```
{% endswagger-response %}
{% endswagger %}
