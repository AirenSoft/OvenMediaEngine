# TranscodeWebhook

TranscodeWebhook allows OvenMediaEngine to use OutputProfiles from the Control Server's response instead of the OutputProfiles in the local configuration (Server.xml). OvenMediaEngine requests OutputProfiles from the Control Server when streams are created, enabling the specification of different profiles for each individual stream.

\


<figure><img src="../.gitbook/assets/image.png" alt=""><figcaption></figcaption></figure>

## Configuration

```
<Applications>
	<Application>
		<Name>app</Name>
		<!-- Application type (live/vod) -->
		<Type>live</Type>

		<TranscodeWebhook>
			<Enable>true</Enable>
			<ControlServerUrl>http://example.com/webhook</ControlServerUrl>
			<SecretKey>abc123!@#</SecretKey>
			<Timeout>1500</Timeout>
			<UseLocalProfilesOnConnectionFailure>true</UseLocalProfilesOnConnectionFailure>
                        <UseLocalProfilesOnServerDisallow>false</UseLocalProfilesOnServerDisallow>
                        <UseLocalProfilesOnErrorResponse>false</UseLocalProfilesOnErrorResponse>
		</TranscodeWebhook>
```

**Enable (required)**\
&#x20;   You can enable or disable TranscodeWebhook settings.

**ControlServerUrl (required)**\
&#x20;   It's the URL of the Control Server, and it supports both HTTP and HTTPS.

**SecretKey (optional)**\
&#x20;   This is the Secret Key used to pass authentication for the Control Server. To pass security authentication, an HMAC-SHA1 encrypted value of the HTTP Payload is added to the HTTP Header's X-OME-Signature. This Key is used for generating this value.

**Timeout (optional, default: 1500)**\
&#x20;   This is the Timeout value used when connecting to the Control Server.

**UseLocalProfilesOnConnectionFailure(optional, default: true)**\
&#x20;   This determines whether to use the OutputProfiles from Local settings in case of communication failure with the Control Server. If it's set to "false," a communication failure with the Control Server will result in a failure to create the Output stream.

**UseLocalProfilesOnServerDisallow (optional, default: false)**\
&#x20;   When the Control Server responds with a 200 OK, but "allowed" is set to "false," this policy is followed.

**UseLocalProfilesOnErrorResponse (optional, default: false)**\
&#x20;   When the Control Server responds with error status codes such as 400 Bad Request, 404 Not Found, 500 Internal Error, OvenMediaEngine follows this policy.

## Protocol

### **Request (OME → Control Server)**

OvenMediaEngine sends requests to the Control Server in the following format.

```
POST /configured/target/url/ HTTP/1.1
Content-Length: 944
Content-Type: application/json
Accept: application/json
X-OME-Signature: f871jd991jj1929jsjd91pqa0amm1
{
	"source" : "TCP://192.168.0.200:8843",
	"stream" : 
	{
    	"name" : "stream",
		"virtualHost" : "default",
		"application" : "app",
		"sourceType" : "Rtmp",
		"createdTime" : "2023-10-24T18:10:51.120+09:00",
		"sourceUrl" : "TCP://192.168.0.200:8843",
		"tracks" : 
		[
			{
				"id" : 0,
				"name" : "Video",
				"type" : "Video",
				"video" : 
				{
					"bitrate" : "2500000",
					"bypass" : false,
					"codec" : "H264",
					"framerate" : 30.0,
					"hasBframes" : false,
					"height" : 720,
					"keyFrameInterval" : 0,
					"width" : 1280
				}
			},
			{
				"audio" : 
				{
					"bitrate" : "160000",
					"bypass" : false,
					"channel" : 2,
					"codec" : "AAC",
					"samplerate" : 44100
				},
				"id" : 1,
				"name" : "Audio",
				"type" : "Audio"
			},
			{
				"id" : 2,
				"name" : "Data",
				"type" : "Data"
			}
		]
	}
}
```

### **Response (Control Server → OME)**

The Control Server responds in the following format to specify OutputProfiles for the respective stream.

```
HTTP/1.1 200 OK
Content-Length: 102
Content-Type: application/json
Connection: Closed
{
  "allowed": true,
  "reason": "it will be output to the log file when "allowed" is false",
  "outputProfiles" : {
    "hardwareAcceleration": true,
    "outputProfile":[
      {
        "name": "bypass",
        "outputStreamName": "${OriginStreamName}",
        "playlists": [
          {
            "name": "default",
            "fileName": "default",
            "renditions":[
              {
                "name": "bypass",
                "video": "bypass_video",
                "audio": "bypass_audio"
              }
            ]
          }
        ],
        "encodes": {
          "videos": [
            {
              "name": "bypass_video",
              "bypass": true
            }
          ],
          "audios": [
            {
              "name": "bypass_audio",
              "bypass": true
            }
          ]
        }
      }
    ]
  }
}
```

The "outputProfiles" in the JSON is identical to the configuration in Server.xml, and the format is as follows.

```
"outputProfiles": {
    "hardwareAcceleration": true,
    "outputProfile": [
        {
            "name": "bypass",
            "outputStreamName": "${OriginStreamName}",
            "encodes": {
                "audios": [
                    {
                        "name": "bypass_audio",
                        "bypass": true,
                        "active": true,
                        "codec": "",
                        "bitrate": "",
                        "samplerate": 0,
                        "channel": 0,
                        "bypassIfMatch": {
                            "codec": "",
                            "bitrate": "",
                            "framerate": "",
                            "width": "",
                            "height": "",
                            "sar": "",
                            "samplerate": "",
                            "channel": ""
                        }
                    }
                ],
                "videos": [
                    {
                        "name": "bypass_video",
                        "bypass": true,
                        "active": true,
                        "codec": "",
                        "profile": "",
                        "bitrate": "",
                        "width": 0,
                        "height": 0,
                        "framerate": 0.000000,
                        "preset": "",
                        "threadCount": -1,
                        "keyFrameInterval": 0,
                        "bFrames": 0,
                        "bypassIfMatch": {
                            "codec": "",
                            "bitrate": "",
                            "framerate": "",
                            "width": "",
                            "height": "",
                            "sar": "",
                            "samplerate": "",
                            "channel": ""
                        }
                    }
                ],
                "images": [
                    {
                        "name": "bypass_video",
                        "bypass": true,
                        "active": true,
                        "codec": "",
                        "width": 0,
                        "height": 0,
                        "framerate": 0.000000
                    }
                ]
            },
            "playlists": [
                {
                    "name": "default",
                    "fileName": "default",
                    "options": {
                        "webRtcAutoAbr": true,
                        "hlsChunklistPathDepth": -1
                    },
                    "rendition/renditions": [
                        {
                            "name/name": "bypass",
                            "video/video": "bypass_video",
                            "audio/audio": "bypass_audio"
                        }
                    ]
                }
            ]
        }
    ]
}
```

