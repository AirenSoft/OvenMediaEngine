# Current

{% api-method method="get" host="http://<OME\_HOST>:<API\_PORT>" path="/v1/stats/current/vhosts/{vhost\_name}" %}
{% api-method-summary %}
/v1/stats/current/vhosts/{vhost\_name}
{% endapi-method-summary %}

{% api-method-description %}
Usage statistics of the `VirtualHost`  
  
Request Example:  
`GET http://1.2.3.4:8081/v1/stats/current/vhosts/default`
{% endapi-method-description %}

{% api-method-spec %}
{% api-method-request %}
{% api-method-path-parameters %}
{% api-method-parameter required=true name="vhost\_name" type="string" %}
A name of `VirtualHost`
{% endapi-method-parameter %}
{% endapi-method-path-parameters %}

{% api-method-query-parameters %}
{% api-method-parameter required=true name="access\_token" type="string" %}
A token for authentication
{% endapi-method-parameter %}
{% endapi-method-query-parameters %}
{% endapi-method-request %}

{% api-method-response %}
{% api-method-response-example httpCode=200 %}
{% api-method-response-example-description %}
Returns a statistics of the `VirtualHost`.
{% endapi-method-response-example-description %}

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
{% endapi-method-response-example %}

{% api-method-response-example httpCode=404 %}
{% api-method-response-example-description %}
Not Found
{% endapi-method-response-example-description %}

```
{
	"code": 404,
	"message": "Could not find the virtual host: [non-exists] (404)"
}
```
{% endapi-method-response-example %}
{% endapi-method-response %}
{% endapi-method-spec %}
{% endapi-method %}

{% api-method method="get" host="http://<OME\_HOST>:<API\_PORT>" path="/v1/stats/current/vhosts/{vhost\_name}/apps/{app\_name}" %}
{% api-method-summary %}
/v1/stats/current/vhosts/{vhost\_name}/apps/{app\_name}
{% endapi-method-summary %}

{% api-method-description %}
Usage statistics of the `Application`  
  
Request Example:  
`GET http://1.2.3.4:8081/v1/stats/current/vhosts/default/apps/app`
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

{% api-method-query-parameters %}
{% api-method-parameter required=true name="access\_token" type="string" %}
A token for authentication
{% endapi-method-parameter %}
{% endapi-method-query-parameters %}
{% endapi-method-request %}

{% api-method-response %}
{% api-method-response-example httpCode=200 %}
{% api-method-response-example-description %}
Returns a statistics of the `Application`.
{% endapi-method-response-example-description %}

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
{% endapi-method-response-example %}

{% api-method-response-example httpCode=404 %}
{% api-method-response-example-description %}
Not Found
{% endapi-method-response-example-description %}

```
{
	"code": 404,
	"message": "Could not find the application: [default/non-exists] (404)"
}
```
{% endapi-method-response-example %}
{% endapi-method-response %}
{% endapi-method-spec %}
{% endapi-method %}

{% api-method method="get" host="http://<OME\_HOST>:<API\_PORT>" path="/v1/stats/current/vhosts/{vhost\_name}/apps/{app\_name}/streams/{stream}" %}
{% api-method-summary %}
/v1/stats/current/vhosts/{vhost\_name}/apps/{app\_name}/streams/{stream}
{% endapi-method-summary %}

{% api-method-description %}
Usage statistics of the `Stream`  
  
Request Example:  
`GET http://1.2.3.4:8081/v1/stats/current/vhosts/default/apps/app/streams/{stream}`
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

{% api-method-query-parameters %}
{% api-method-parameter required=true name="access\_token" type="string" %}
A token for authentication
{% endapi-method-parameter %}
{% endapi-method-query-parameters %}
{% endapi-method-request %}

{% api-method-response %}
{% api-method-response-example httpCode=200 %}
{% api-method-response-example-description %}
Returns a statistics of the `Stream`.
{% endapi-method-response-example-description %}

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
{% endapi-method-response-example %}

{% api-method-response-example httpCode=404 %}
{% api-method-response-example-description %}
Not Found
{% endapi-method-response-example-description %}

```
{
	"code": 404,
	"message": "Could not find the stream: [default/app/non-exists] (404)"
}
```
{% endapi-method-response-example %}
{% endapi-method-response %}
{% endapi-method-spec %}
{% endapi-method %}

