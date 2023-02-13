# Push

{% swagger baseUrl="http://<OME_HOST>:<API_PORT>" path="/v1/vhosts/{vhost_name}/apps/{app_name}:startPush" method="post" summary="/v1/vhosts/{vhost_name}/apps/{app_name}:startPush" %}
{% swagger-description %}
This is an action to request a push of a selected stream. Please refer to the "Push" document for detail setting.

\




\


Request Example:

\




`POST http://1.2.3.4:8081/v1/vhosts/default/apps/app:startPush                               `

\


`{`

\


`   "id": "{UserDefinedUniqueId}", `

\


`   "stream": { `

\


`     "name": "output_stream_name", `

\


`     "tracks": [ 101, 102 ] `

\


`  },`

\


`  "protocol": "rtmp",`

\


`  "url":"rtmp://{host}[:port]/{appName}",`

\


`   "streamKey":"{streamName}" `

\


`}`



`POST http://1.2.3.4:8081/v1/vhosts/default/apps/app:startPush                               `

\


`{`

\


`   "id": "{UserDefinedUniqueId}", `

\


`   "stream": { `

\


`     "name": "output_stream_name", `

\


`     "tracks": [ 101, 102 ] `

\


`  },`

\


`  "protocol": "mpegts",`

\


`  "url":"udp://{host}[:port]",`

\


`   "streamKey":"" `

\


`}`
{% endswagger-description %}

{% swagger-parameter in="path" name="vhost_name" type="string" required="true" %}
A name of 

`VirtualHost`
{% endswagger-parameter %}

{% swagger-parameter in="path" name="app_name" type="string" required="true" %}
A name of 

`Application`
{% endswagger-parameter %}

{% swagger-parameter in="header" name="authorization" type="string" required="true" %}
A string for authentication in `Basic Base64(AccessToken)` format.

For example, `Basic b21lLWFjY2Vzcy10b2tlbg==` if access token is `ome-access-token`
{% endswagger-parameter %}

{% swagger-parameter in="body" name="id" type="string" required="true" %}
Unique identifier for push management. if there is no value,  automatically created and returned
{% endswagger-parameter %}

{% swagger-parameter in="body" name="stream" type="string" required="true" %}
Output stream for push
{% endswagger-parameter %}

{% swagger-parameter in="body" name="name" type="string" required="true" %}
Output stream name
{% endswagger-parameter %}

{% swagger-parameter in="body" name="tracks" type="string" %}
Track id for want to push, if there is no value, all tracks are push
{% endswagger-parameter %}

{% swagger-parameter in="body" name="protocol" type="string" required="true" %}
Transport protocol [rtmp | mpegts]
{% endswagger-parameter %}

{% swagger-parameter in="body" name="url" type="string" required="true" %}
Destination URL
{% endswagger-parameter %}

{% swagger-parameter in="body" name="streamKey" type="object" required="true" %}
Destination stream key
{% endswagger-parameter %}

{% swagger-response status="200" description="Success" %}
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
      "streamKey": "2u5w-rt7q-tepj-91qa-4yft",
      "totalsentBytes": 0,
      "totalsentTime": 0,
      "url": "rtmp://a.rtmp.youtube.com/live2",
      "vhost": "default"
    }
  ],
  "statusCode": 200
}
```
{% endswagger-response %}

{% swagger-response status="400" description="Invalid Parameters" %}
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
{% endswagger-response %}
{% endswagger %}

{% swagger baseUrl="http://<OME_HOST>:<API_PORT>" path="/v1/vhosts/{vhost_name}/apps/{app_name}:stopPush" method="post" summary="/v1/vhosts/{vhost_name}/apps/{app_name}:stopPush" %}
{% swagger-description %}
Request to stop pushing

\




\


Request Example:

\




`POST http://1.2.3.4:8081/v1/vhosts/default/apps/app:stopRecord`

\


``

\


`{`

\


`   "id": "{userDefinedUniqueId}" `

\


`}`
{% endswagger-description %}

{% swagger-parameter in="path" name="vhost_name" type="string" required="true" %}
A name of 

`VirtualHost`
{% endswagger-parameter %}

{% swagger-parameter in="path" name="app_name" type="string" required="true" %}
A name of 

`Application`
{% endswagger-parameter %}

{% swagger-parameter in="header" name="authorization" type="string" required="true" %}
A string for authentication in `Basic Base64(AccessToken)` format.

For example, `Basic b21lLWFjY2Vzcy10b2tlbg==` if access token is `ome-access-token`.
{% endswagger-parameter %}

{% swagger-parameter in="body" name="id" type="string" required="true" %}
Unique identifier for push management
{% endswagger-parameter %}

{% swagger-response status="200" description="Success" %}
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
      "streamKey": "2u5w-rt7q-tepj-91qa-4yft",
      "totalsentBytes": 32841631,
      "totalsentTime": 601000,
      "url": "rtmp://a.rtmp.youtube.com/live2",
      "vhost": "default"
    
```
{% endswagger-response %}

{% swagger-response status="400" description="Invalid Parameters" %}
```
{
	"statusCode": 400,
	"message": "There is no required parameter [id] (400)"
}
```
{% endswagger-response %}

{% swagger-response status="404" description="No content" %}
```
{
	"statusCode": 404,
	"message": "There is no push information related to the ID [{id}] (404)"
}
```
{% endswagger-response %}
{% endswagger %}

{% swagger baseUrl="http://<OME_HOST>:<API_PORT>" path="/v1/vhosts/{vhost_name}/apps/{app_name}:pushes" method="post" summary="/v1/vhosts/{vhost_name}/apps/{app_name}:pushes" %}
{% swagger-description %}
Get all push lists for a specific application

\




\


Request Example:

\




`POST http://1.2.3.4:8081/v1/vhosts/default/apps/app:pushes`
{% endswagger-description %}

{% swagger-parameter in="path" name="vhost_name" type="string" required="true" %}
A name of 

`VirtualHost`
{% endswagger-parameter %}

{% swagger-parameter in="path" name="app_name" type="string" required="true" %}
A name of 

`Application`
{% endswagger-parameter %}

{% swagger-parameter in="header" name="authorization" type="string" required="true" %}
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

{% swagger-response status="200" description="Success" %}
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
      "streamKey": "2u5w-rt7q-tepj-91qa-4yft",
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
      "streamKey": "2u5w-rt7q-tepj-91qa-4yft",
      "totalsentBytes": 0,
      "totalsentTime": 0,
      "url": "rtmp://a.rtmp.youtube.com/live2",
      "vhost": "default"
    }
	]
}
```
{% endswagger-response %}

{% swagger-response status="204" description="Not Found" %}
```
{
	"code": 204,
	"message": "There is no pushes information (204)"
}
```
{% endswagger-response %}
{% endswagger %}
