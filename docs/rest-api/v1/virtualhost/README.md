# VirtualHost

{% swagger baseUrl="http://<OME_HOST>:<API_PORT>" path="/v1/vhosts" method="get" summary="/v1/vhosts" %}
{% swagger-description %}
Lists all virtual host names

\




\


Request Example:

\




`GET http://1.2.3.4:8081/v1/vhosts`
{% endswagger-description %}

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
Returns a list of virtual host names" %}
```
{
	"statusCode": 200,
	"message": "OK",
	"response": [
		"default"
	]
}
```
{% endswagger-response %}
{% endswagger %}

{% swagger baseUrl="http://<OME_HOST>:<API_PORT>" path="/v1/vhosts/{vhost_name}" method="get" summary="/v1/vhosts/{vhost_name}" %}
{% swagger-description %}
Gets the configuration of the 

`VirtualHost`

\




\


Request Example:

\




`GET http://1.2.3.4:8081/v1/vhosts/default`
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

{% swagger-response status="200" description="- Return type: Response<VirtualHost>
- Description
Returns the specified virtual host information" %}
```
{
	"statusCode": 200,
	"message": "OK",
	"response": {
		"name": "default",
		"host": {
			"names": [
				"*"
			],
			"tls": {
				"certPath": "airensoft_com.crt",
				"chainCertPath": "airensoft_com_chain.crt",
				"keyPath": "airensoft_com.key"
			}
		}
	}
}
```
{% endswagger-response %}

{% swagger-response status="404" description="- Return type: Response<>
- Description
The specified VirtualHost was not found." %}
```
{
	"statusCode": 404,
	"message": "Could not find the virtual host: [non-exists] (404)"
}
```
{% endswagger-response %}
{% endswagger %}
