# Push

{% api-method method="post" host="http://<OME\_HOST>:<API\_PORT>" path="/v1/vhosts/{vhost\_name}/apps/{app\_name}:startPush" %}
{% api-method-summary %}
/v1/vhosts/{vhost\_name}/apps/{app\_name}:startPush
{% endapi-method-summary %}

{% api-method-description %}
This is an action to request a push of a selected stream. Please refer to the "Push" document for detail setting.  
  
Request Example:  
`GET http://1.2.3.4:8081/v1/vhosts/default/apps/app:startPush                                 
{  
  "id": "{UserDefinedUniqueId}",  
  "stream": {  
    "name": "output_stream_name",  
    "tracks": [ 101, 102 ]  
  },  
  "protocol": "rtmp",  
  "url":"rtmp://{ip}/{appName}",  
  "steramkey":"{streamName}"  
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
{% api-method-parameter name="authorization" type="string" required=true %}
A string for authentication in `Basic Base64(AccessToken)` format.
{% endapi-method-parameter %}

{% api-method-parameter %}
For example, `Basic b21lLWFjY2Vzcy10b2tlbg==` if access token is `ome-access-token`.
{% endapi-method-parameter %}
{% endapi-method-headers %}

{% api-method-body-parameters %}
{% api-method-parameter name="id" type="string" required=false %}
Unique identifier for push management. if there is no value,  automatically created and returned
{% endapi-method-parameter %}

{% api-method-parameter name="stream" type="string" required=true %}
Output stream for push
{% endapi-method-parameter %}

{% api-method-parameter name="name" type="string" required=true %}
Output stream name
{% endapi-method-parameter %}

{% api-method-parameter name="tracks" type="string" required=false %}
Track id for want to push, if there is no value, all tracks are push
{% endapi-method-parameter %}

{% api-method-parameter name="protocol" type="string" required=true %}
Transport protocol \[rtmp \| mpegts\]
{% endapi-method-parameter %}

{% api-method-parameter required=true name="url" type="string" %}
Destination URL
{% endapi-method-parameter %}

{% api-method-parameter required=true name="streamKey" type="object" %}
Destination stream key
{% endapi-method-parameter %}
{% endapi-method-body-parameters %}
{% endapi-method-request %}

{% api-method-response %}
{% api-method-response-example httpCode=200 %}
{% api-method-response-example-description %}

{% endapi-method-response-example-description %}

```
{
  "message": "OK",
  "response": [
    {
      "app": "app",
      "createdTime": "2021-02-04T01:42:40.160+0900",
      "id": "userDefinedUniqueId",
      "protocol": "rtmp",
      "sentBytes": 0,
      "sentTime": 0,
      "sequence": 0,
      "startTime": "1970-01-01T09:00:00.000+0900",
      "state": "ready",
      "stream": {
        "name": "stream",
        "tracks": [
          101,
          102
        ]
      },
      "stream_key": "2u5w-rt7q-tepj-91qa-4yft",
      "totalsentBytes": 0,
      "totalsentTime": 0,
      "url": "rtmp://a.rtmp.youtube.com/live2",
      "vhost": "default"
    }
  ],
  "statusCode": 200
}
```
{% endapi-method-response-example %}

{% api-method-response-example httpCode=400 %}
{% api-method-response-example-description %}

{% endapi-method-response-example-description %}

```
{
	"statusCode": 400,
	"message": "There is no required parameter [{id}, {stream.name}] (100)"
}
	
{
	"statusCode": 400,
	"message": "Duplicate ID already exists [{id}] (400)"
}
```
{% endapi-method-response-example %}
{% endapi-method-response %}
{% endapi-method-spec %}
{% endapi-method %}

{% api-method method="post" host="http://<OME\_HOST>:<API\_PORT>" path="/v1/vhosts/{vhost\_name}/apps/{app\_name}:stopPush" %}
{% api-method-summary %}
/v1/vhosts/{vhost\_name}/apps/{app\_name}:stopPush
{% endapi-method-summary %}

{% api-method-description %}
Stops recording for `Application`  
  
Request Example:  
`GET http://1.2.3.4:8081/v1/vhosts/default/apps/app:stopRecord  
  
{  
  "id": "{userDefinedUniqueId}"  
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
{% api-method-parameter name="authorization" required=true type="string" %}
A string for authentication in `Basic Base64(AccessToken)` format.
{% endapi-method-parameter %}

{% api-method-parameter %}
For example, `Basic b21lLWFjY2Vzcy10b2tlbg==` if access token is `ome-access-token`.
{% endapi-method-parameter %}
{% endapi-method-headers %}

{% api-method-body-parameters %}
{% api-method-parameter required=true name="id" type="string" %}
Unique identifier for push management
{% endapi-method-parameter %}
{% endapi-method-body-parameters %}
{% endapi-method-request %}

{% api-method-response %}
{% api-method-response-example httpCode=200 %}
{% api-method-response-example-description %}

{% endapi-method-response-example-description %}

```
    {
      "id": "userDefinedUniqueId",
      "app": "app",
      "createdTime": "2021-01-18T10:25:03.765+0900",
      "startTime": "2021-01-18T10:25:03.765+0900",
      "finishTime": "2021-01-18T10:35:03.765+0900",
      "protocol": "rtmp",
      "sentBytes": 32841631,
      "sentTime": 601,
      "sequence": 0,
      "state": "stopping",
      "stream": {
        "name": "stream",
        "tracks": [
          101,
          102
        ]
      },
      "stream_key": "2u5w-rt7q-tepj-91qa-4yft",
      "totalsentBytes": 32841631,
      "totalsentTime": 601000,
      "url": "rtmp://a.rtmp.youtube.com/live2",
      "vhost": "default"
    
```
{% endapi-method-response-example %}

{% api-method-response-example httpCode=400 %}
{% api-method-response-example-description %}

{% endapi-method-response-example-description %}

```
{
	"statusCode": 400,
	"message": "There is no required parameter [id] (400)"
}
```
{% endapi-method-response-example %}

{% api-method-response-example httpCode=404 %}
{% api-method-response-example-description %}

{% endapi-method-response-example-description %}

```
{
	"statusCode": 404,
	"message": "There is no push information related to the ID [{id}] (404)"
}
```
{% endapi-method-response-example %}
{% endapi-method-response %}
{% endapi-method-spec %}
{% endapi-method %}

{% api-method method="post" host="http://<OME\_HOST>:<API\_PORT>" path="/v1/vhosts/{vhost\_name}/apps/{app\_name}:pushes" %}
{% api-method-summary %}
/v1/vhosts/{vhost\_name}/apps/{app\_name}:pushes
{% endapi-method-summary %}

{% api-method-description %}
Lists all `Record` in the `Application`  
  
Request Example:  
`POST http://1.2.3.4:8081/v1/vhosts/default/apps/app:pushes`
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
{% api-method-parameter name="authorization" type="string" required=true %}
A string for authentication in `Basic Base64(AccessToken)` format.  
For example, `Basic b21lLWFjY2Vzcy10b2tlbg==` if access token is `ome-access-token`.
{% endapi-method-parameter %}
{% endapi-method-headers %}
{% endapi-method-request %}

{% api-method-response %}
{% api-method-response-example httpCode=200 %}
{% api-method-response-example-description %}

{% endapi-method-response-example-description %}

```
{
	"statusCode": 200,
	"message": "OK",
	"response": [
    {
      "id": "UniqueId1",
      "app": "app",
      "createdTime": "2021-01-18T10:25:03.765+0900",
      "startTime": "1970-01-01T09:00:00.000+0900",
      "finishTime": "1970-01-01T09:00:00.000+0900",
      "protocol": "rtmp",
      "sentBytes": 0,
      "sentTime": 0,
      "sequence": 0,
      "state": "ready",
      "stream": {
        "name": "stream",
        "tracks": [
          101,
          102
        ]
      },
      "stream_key": "2u5w-rt7q-tepj-91qa-4yft",
      "totalsentBytes": 0,
      "totalsentTime": 0,
      "url": "rtmp://a.rtmp.youtube.com/live2",
      "vhost": "default"
    },
    {
      "id": "UniqueId2",
      "app": "app",
      "createdTime": "2021-01-18T10:26:15.968+0900",
      "startTime": "1970-01-01T09:00:00.000+0900",
      "finishTime": "1970-01-01T09:00:00.000+0900",
      "protocol": "rtmp",
      "sentBytes": 0,
      "sentTime": 0,
      "sequence": 0,
      "state": "ready",
      "stream": {
        "name": "strea2m",
        "tracks": [
          101,
          102
        ]
      },
      "stream_key": "2u5w-rt7q-tepj-91qa-4yft",
      "totalsentBytes": 0,
      "totalsentTime": 0,
      "url": "rtmp://a.rtmp.youtube.com/live2",
      "vhost": "default"
    }
	]
}
```
{% endapi-method-response-example %}

{% api-method-response-example httpCode=204 %}
{% api-method-response-example-description %}
Not Found
{% endapi-method-response-example-description %}

```
{
	"code": 204,
	"message": "There is no pushes information (204)"
}
```
{% endapi-method-response-example %}
{% endapi-method-response %}
{% endapi-method-spec %}
{% endapi-method %}

