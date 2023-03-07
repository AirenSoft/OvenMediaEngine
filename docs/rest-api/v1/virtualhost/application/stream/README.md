# Stream

## Get Stream List

> #### <mark style="color:blue;">**GET**</mark> /v1/vhosts/{vhost name}/apps/{app name}/streams

Get all stream names in the {vhost name}/{app name} application.&#x20;

### **Header**

```
Authorization: Basic {credentials}
```

_Authorization_

 Credentials for HTTP Basic Authentication created with \<AccessToken>

### Responses

<details>

<summary>200 Ok</summary>

The request has succeeded

**Header**

```
Content-Type: application/json
```

**Body**

<pre class="language-json"><code class="lang-json">{
<strong>    "statusCode": 200,
</strong>    "message": "OK",
    "response": [
        "stream",
        "stream2"
    ]
}
</code></pre>

_statusCode_

 Same as HTTP Status Code

_message_

 A human-readable description of the response code

_response_

 Json array containing a list of stream names

</details>

<details>

<summary>404 Not Found</summary>

The given vhost name or app name could not be found.

**Header**

```json
Content-Type: application/json
```

**Body**

```json
{
    "statusCode": 404,
    "message": "Could not find the application: [default/non-exists] (404)"
}
```

</details>

