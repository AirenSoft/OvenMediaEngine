# Current

{% swagger baseUrl="http://<OME_HOST>:<API_PORT>" path="/v1/stats/current/vhosts/{vhost_name}" method="get" summary="/v1/stats/current/vhosts/{vhost_name}" %}
{% swagger-description %}
Usage statistics of the 

`VirtualHost`

\




\


Request Example:

\




`GET http://1.2.3.4:8081/v1/stats/current/vhosts/default`
{% endswagger-description %}

{% swagger-parameter in="path" name="vhost_name" type="string" %}
A name of 

`VirtualHost`
{% endswagger-parameter %}

{% swagger-parameter in="query" name="access_token" type="string" %}
A token for authentication
{% endswagger-parameter %}

{% swagger-response status="200" description="Returns a statistics of the VirtualHost." %}
```
{
	"createdTime": "2021-01-11T02:52:22.013+09:00",
	"lastRecvTime": "2021-01-11T04:11:41.734+09:00",
	"lastSentTime": "2021-01-11T02:52:22.013+09:00",
	"lastUpdatedTime": "2021-01-11T04:11:41.734+09:00",
	"maxTotalConnectionTime": "2021-01-11T02:52:22.013+09:00",
	"maxTotalConnections": 0,
	"totalBytesIn": 494713880,
	"totalBytesOut": 0,
	"totalConnections": 0
}
```
{% endswagger-response %}

{% swagger-response status="404" description="Not Found" %}
```
{
	"code": 404,
	"message": "Could not find the virtual host: [non-exists] (404)"
}
```
{% endswagger-response %}
{% endswagger %}

{% swagger baseUrl="http://<OME_HOST>:<API_PORT>" path="/v1/stats/current/vhosts/{vhost_name}/apps/{app_name}" method="get" summary="/v1/stats/current/vhosts/{vhost_name}/apps/{app_name}" %}
{% swagger-description %}
Usage statistics of the 

`Application`

\




\


Request Example:

\




`GET http://1.2.3.4:8081/v1/stats/current/vhosts/default/apps/app`
{% endswagger-description %}

{% swagger-parameter in="path" name="vhost_name" type="string" %}
A name of 

`VirtualHost`
{% endswagger-parameter %}

{% swagger-parameter in="path" name="app_name" type="string" %}
A name of 

`Application`
{% endswagger-parameter %}

{% swagger-parameter in="query" name="access_token" type="string" %}
A token for authentication
{% endswagger-parameter %}

{% swagger-response status="200" description="Returns a statistics of the Application." %}
```
{
	"createdTime": "2021-01-11T03:39:38.802+09:00",
	"lastRecvTime": "2021-01-11T04:14:40.092+09:00",
	"lastSentTime": "2021-01-11T03:39:38.802+09:00",
	"lastUpdatedTime": "2021-01-11T04:14:40.092+09:00",
	"maxTotalConnectionTime": "2021-01-11T03:39:38.802+09:00",
	"maxTotalConnections": 0,
	"totalBytesIn": 550617693,
	"totalBytesOut": 0,
	"totalConnections": 0
}
```
{% endswagger-response %}

{% swagger-response status="404" description="Not Found" %}
```
{
	"code": 404,
	"message": "Could not find the application: [default/non-exists] (404)"
}
```
{% endswagger-response %}
{% endswagger %}

{% swagger baseUrl="http://<OME_HOST>:<API_PORT>" path="/v1/stats/current/vhosts/{vhost_name}/apps/{app_name}/streams/{stream}" method="get" summary="/v1/stats/current/vhosts/{vhost_name}/apps/{app_name}/streams/{stream}" %}
{% swagger-description %}
Usage statistics of the 

`Stream`

\




\


Request Example:

\




`GET http://1.2.3.4:8081/v1/stats/current/vhosts/default/apps/app/streams/{stream}`
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

{% swagger-parameter in="query" name="access_token" type="string" %}
A token for authentication
{% endswagger-parameter %}

{% swagger-response status="200" description="Returns a statistics of the Stream." %}
```
{
	"createdTime": "2021-01-11T03:39:38.802+09:00",
	"lastRecvTime": "2021-01-11T04:14:40.092+09:00",
	"lastSentTime": "2021-01-11T03:39:38.802+09:00",
	"lastUpdatedTime": "2021-01-11T04:14:40.092+09:00",
	"maxTotalConnectionTime": "2021-01-11T03:39:38.802+09:00",
	"maxTotalConnections": 0,
	"totalBytesIn": 550617693,
	"totalBytesOut": 0,
	"totalConnections": 0
}
```
{% endswagger-response %}

{% swagger-response status="404" description="Not Found" %}
```
{
	"code": 404,
	"message": "Could not find the stream: [default/app/non-exists] (404)"
}
```
{% endswagger-response %}
{% endswagger %}
