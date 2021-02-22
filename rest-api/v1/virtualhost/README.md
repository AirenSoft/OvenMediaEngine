# VirtualHost

{% api-method method="get" host="http://<OME\_HOST>:<API\_PORT>" path="/v1/vhosts" %}
{% api-method-summary %}
/v1/vhosts
{% endapi-method-summary %}

{% api-method-description %}
Lists all virtual host names  
  
Request Example:  
`GET http://1.2.3.4:8081/v1/vhosts`
{% endapi-method-description %}

{% api-method-spec %}
{% api-method-request %}
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
Returns a list of virtual host names
{% endapi-method-response-example-description %}

```
{
	"statusCode": 200,
	"message": "OK",
	"response": [
		"default"
	]
}
```
{% endapi-method-response-example %}
{% endapi-method-response %}
{% endapi-method-spec %}
{% endapi-method %}

{% api-method method="get" host="http://<OME\_HOST>:<API\_PORT>" path="/v1/vhosts/{vhost\_name}" %}
{% api-method-summary %}
/v1/vhosts/{vhost\_name}
{% endapi-method-summary %}

{% api-method-description %}
Gets the configuration of the `VirtualHost`  
  
Request Example:  
`GET http://1.2.3.4:8081/v1/vhosts/default`
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
{% endapi-method-request %}

{% api-method-response %}
{% api-method-response-example httpCode=200 %}
{% api-method-response-example-description %}
- Return type: Response&lt;`VirtualHost`&gt;  
- Description  
Returns the specified virtual host information
{% endapi-method-response-example-description %}

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
{% endapi-method-response-example %}

{% api-method-response-example httpCode=404 %}
{% api-method-response-example-description %}
- Return type: Response&lt;&gt;  
- Description  
The specified `VirtualHost` was not found.
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

