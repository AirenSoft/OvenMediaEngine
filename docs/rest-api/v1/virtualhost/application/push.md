# Push

{% swagger baseUrl="http://<OME_HOST>:<API_PORT>" path="/v1/vhosts/{vhost_name}/apps/{app_name}:startPush" method="post" summary="Start push publishing" %}
{% swagger-description %}
**Example - RTMP push publishing by Output Stream Name**\
`POST http[s]://{host}/v1/vhosts/default/apps/app:startPush`\
`{`\
&#x20; `"id": "{unique_push_id}",`\
&#x20; `"stream": {`\
&#x20;   `"name": "{output_stream_name}"`\
&#x20; `},`\
&#x20; `"protocol": "rtmp",`\
&#x20; `"url":"rtmp://{host}[:port]/{app_ame}",`\
&#x20; `"streamKey":"{stream_name}"`\
`}`

**Example - MPEG TS push publishing by Output Stream Name**\
`POST http[s]://{host}/v1/vhosts/default/apps/app:startPush`\
`{`\
&#x20; `"id": "{unique_push_id}",`\
&#x20; `"stream": {`\
&#x20;   `"name": "{output_stream_name}"`\
&#x20; `},`\
&#x20; `"protocol": "mpegts",`\
&#x20; `"url":"udp://{host}[:port]",`\
&#x20; `"streamKey":""`\
`}`

**Example - Push publishing by Output Stream Name and Track Ids**\
`POST http[s]://{host}/v1/vhosts/default/apps/app:startPush`\
`{`\
&#x20; `"id": "{unique_push_id}",`\
&#x20; `"stream": {`\
&#x20;   `"name": "{output_stream_name}",`\
&#x20;   `"trackIds": [ 101, 102 ]`\
&#x20; `},`\
&#x20; `"protocol": "rtmp",`\
&#x20; `"url":"rtmp://{host}[:port]/{appName}",`\
&#x20; `"streamKey":"{stream_name}"`\
`}`

**Example - Push publishing by Output Stream Name and Variant Names**\
`POST http[s]://{host}/v1/vhosts/default/apps/app:startPush`\
`{`\
&#x20; `"id": "{unique_push_id}",`\
&#x20; `"stream": {`\
&#x20;   `"name": "{output_stream_name}",`\
&#x20;   `"variantNames": [ "h264_fhd", "aac" ]`\
&#x20; `},`\
&#x20; `"protocol": "rtmp",`\
&#x20; `"url":"rtmp://{host}[:port]/{app_name}",`\
&#x20; `"streamKey":"{stream_name}"`\
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
Unique identifier of push publishing
{% endswagger-parameter %}

{% swagger-parameter in="body" name="stream" type="string" required="true" %}
Output stream for push.
{% endswagger-parameter %}

{% swagger-parameter in="body" name="name" type="string" required="true" %}
Output stream name
{% endswagger-parameter %}

{% swagger-parameter in="body" name="trackIds" type="array" required="false" %}
Used for push publishing specific track ids.
{% endswagger-parameter %}

{% swagger-parameter in="body" name="variantNames" type="array" %}
Used for push publishing specific variant names.
{% endswagger-parameter %}

{% swagger-parameter in="body" name="protocol" type="string" required="true" %}
Transport protocol [rtmp | mpegts]
{% endswagger-parameter %}

{% swagger-parameter in="body" name="url" type="string" required="true" %}
Destination URL.
{% endswagger-parameter %}

{% swagger-parameter in="body" name="streamKey" type="object" required="true" %}
Destination stream key.
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

{% swagger baseUrl="http://<OME_HOST>:<API_PORT>" path="/v1/vhosts/{vhost_name}/apps/{app_name}:stopPush" method="post" summary="Stop push publishing" %}
{% swagger-description %}
**Example**

\




`POST http[s]://{host}/v1/vhosts/default/apps/app:stopRecord`

\


`{`

\


  

`"id": "{unique_push_id}"`

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
Unique identifier of push publishing
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

{% swagger baseUrl="http://<OME_HOST>:<API_PORT>" path="/v1/vhosts/{vhost_name}/apps/{app_name}:pushes" method="post" summary="Push publishing status" %}
{% swagger-description %}


**Example**

`POST http[s]://{host}/v1/vhosts/default/apps/app:pushes`\
`{`\
&#x20; `"id": "{unique_push_id}"`\
`}`\
``
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

\\

For example,

`Basic b21lLWFjY2Vzcy10b2tlbg==`

if access token is

`ome-access-token`

.
{% endswagger-parameter %}

{% swagger-parameter in="body" name="id" type="string" %}
Unique identifier of push publishing
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
