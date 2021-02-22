# Recording

{% api-method method="post" host="http://<OME\_HOST>:<API\_PORT>" path="/v1/vhosts/{vhost\_name}/apps/{app\_name}:startRecord" %}
{% api-method-summary %}
/v1/vhosts/{vhost\_name}/apps/{app\_name}:startRecord
{% endapi-method-summary %}

{% api-method-description %}
This is an action that requests recording of the output stream. The output file path can be set through API. If not set, the default output file path set in FILE Publisher in Server.xml is used. For information on settings, see the "Recording" document. You can also select the track to record by inquiring the Track ID through the stream inquiry API. If not selected, all tracks are recorded in one file.  
  
Request Example:  
`POST http://1.2.3.4:8081/v1/vhosts/default/apps/app:startRecord  
  
{  
  "id": "userDefinedUniqueId",  
  "stream": {  
    "name": "outputStreamName",  
    "tracks": [  
      101,  
      102  
    ]  
  },  
  "filePath" : "{/path/to/save/recorded/file.ts}",  
  "infoPath" : "{/Path/to/save/information/file.xml}"  
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
{% api-method-parameter name="id" type="string" required=true %}
An unique identifier for recording management
{% endapi-method-parameter %}

{% api-method-parameter name="stream" type="string" required=true %}
Output stream for recording
{% endapi-method-parameter %}

{% api-method-parameter required=true name="name" type="string" %}
Output stream name
{% endapi-method-parameter %}

{% api-method-parameter required=false name="tracks" type="array" %}
Track id for want to recording. If there is no value, all tracks are recorded
{% endapi-method-parameter %}

{% api-method-parameter required=false name="filePath" type="string" %}
Path to save recorded file
{% endapi-method-parameter %}

{% api-method-parameter required=false name="infoPath" type="string" %}
Path to save information file
{% endapi-method-parameter %}
{% endapi-method-body-parameters %}
{% endapi-method-request %}

{% api-method-response %}
{% api-method-response-example httpCode=200 %}
{% api-method-response-example-description %}

{% endapi-method-response-example-description %}

```
{
	"statusCode": 200,
	"message": "Success",
  "response": [
    {
      "app": "app",
      "createdTime": "2021-02-03T10:52:22.037+0900",
      "id": "{userDefinedUniqueId}",
      "recordBytes": 0,
      "recordTime": 0,
      "sequence": 0,
      "state": "ready",
      "stream": {
         "name": "{outputStreamName}",
         "tracks": [
          101,
          102
         ]
      },
      "totalRecordBytes": 0,
      "totalRecordTime": 0,
      "vhost": "default",
      "filePath" : "{/path/to/save/recorded/file.ts}",
      "infoPath" : "{/Path/to/save/information/file.xml}"
    }
  ]
}
```
{% endapi-method-response-example %}

{% api-method-response-example httpCode=400 %}
{% api-method-response-example-description %}

{% endapi-method-response-example-description %}

```
{
	"statusCode": 400,
	"message": "There is no required parameter [{id}, {stream.name}] (400)"
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

{% api-method method="post" host="http://<OME\_HOST>:<API\_PORT>" path="/v1/vhosts/{vhost\_name}/apps/{app\_name}:stopRecord" %}
{% api-method-summary %}
/v1/vhosts/{vhost\_name}/apps/{app\_name}:stopRecord
{% endapi-method-summary %}

{% api-method-description %}
The action requesting the recording to stop. Use the id parameters used when requested.  
  
Request Example:  
`GET http://1.2.3.4:8081/v1/vhosts/default/apps/app:stopRecord  
  
{  
  "id": "userDefinedUniqueId"  
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
{% api-method-parameter required=true name="id" type="string" %}
An unique identifier for recording c
{% endapi-method-parameter %}
{% endapi-method-body-parameters %}
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
			"id": "{userDefinedUniqueId}",
			"vhost": "default",
	    "app": "app",
			"stream": {
				"name": "stream2_o",
				"tracks": [
					101,
					102
				]
			},
      "createdTime": "2021-01-18T03:27:16.019+09:00",
			"startTime": "2021-01-18T03:27:16.019+09:00",
			"finishTime": "2021-01-19T04:27:16.019+09:00",
      "filePath" : "{/path/to/save/recorded/file.ts}",
      "infoPath" : "{/Path/to/save/information/file.xml}"			
			"state": "Stopping"
		}
	]
}
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
	"message": "There is no record information related to the ID [{id}] (404)"
}
```
{% endapi-method-response-example %}
{% endapi-method-response %}
{% endapi-method-spec %}
{% endapi-method %}

{% api-method method="post" host="http://<OME\_HOST>:<API\_PORT>" path="/v1/vhosts/{vhost\_name}/apps/{app\_name}:records" %}
{% api-method-summary %}
/v1/vhosts/{vhost\_name}/apps/{app\_name}:records
{% endapi-method-summary %}

{% api-method-description %}
You can view all the recording lists that are being recorded in the application. Information such as recording status, file path, size, and recording time can be found in the inquired record item.  
  
Request Example:  
`POST http://1.2.3.4:8081/v1/vhosts/default/apps/app:records`
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

{% endapi-method-response-example-description %}

```
{
    "message": "OK",
    "response": [
		{
			"id": "UserDefinedUniqueId",
			"app": "app",
			"createdTime": "2021-01-18T03:27:16.019+09:00",
			"finishTime": "1970-01-01T09:00:00+09:00",
			"recordBytes": 0,
			"recordTime": 0,
			"sequence": 0,
			"startTime": "1970-01-01T09:00:00+09:00",
			"state": "ready",
      "filePath" : "{/path/to/save/recorded/file.ts}",
      "infoPath" : "{/Path/to/save/information/file.xml}"			 
			"stream": {
				"name": "stream2_o",
				"tracks": [
					101,
					102
				]
			},
			"totalRecordBytes": 0,
			"totalRecordTime": 0,
			"vhost": "default"
		},
		{
			"id": "UserDefinedUniqueId2",
			"app": "app",
			"createdTime": "2021-01-18T03:24:31.812+09:00",
			"finishTime": "1970-01-01T09:00:00+09:00",
			"recordBytes": 0,
			"recordTime": 0,
			"sequence": 0,
			"startTime": "1970-01-01T09:00:00+09:00",
			"state": "ready",
      "filePath" : "{/path/to/save/recorded/file.ts}",
      "infoPath" : "{/Path/to/save/information/file.xml}"			
			"stream": {
				"name": "stream_o",
				"tracks": [
					101,
					102
				]
			},
			"totalRecordBytes": 0,
			"totalRecordTime": 0,
			"vhost": "default"
		}
	]
}
```
{% endapi-method-response-example %}

{% api-method-response-example httpCode=204 %}
{% api-method-response-example-description %}

{% endapi-method-response-example-description %}

```
{
	"statusCode": 204,
	"message": "There is no record information (204)"
}
```
{% endapi-method-response-example %}
{% endapi-method-response %}
{% endapi-method-spec %}
{% endapi-method %}

