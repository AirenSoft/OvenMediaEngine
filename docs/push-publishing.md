# Push Publishing

&#x20;OvenMediaEngine supports **Push Publishing** function that can restreaming live streams to other systems. The protocol supported for retransmission uses **SRT**, **RTMP** and **MPEGTS**. Because, most services and products support this protocol. &#x20;

The StreamMap feature has been added, and it now automatically restreaming based on predefined conditions. You can also use the Rest API to control and monitor it.

## Configuration

### Push Publisher

To use Push Publishing, you need to declare the **`<Push>`** publisher in the configuration. **\<StreamMap>** is optional. It is used when automatic push is needed.

```xml
<Applications>
  <Application>
     ...
    <Publishers>
      ... 
      <Push>
         <!-- [Optional] -->
         <StreamMap>
           <Enable>false</Enable>
           <Path>path/to/map.xml</Path>
         </StreamMap>
      </Push>
      ...
    </Publishers>
  </Application>
</Applications>
```

{% hint style="info" %}
The RTMP protocol only supports H264 and AAC codecs.
{% endhint %}

### StreamMap

**\<StreamMap>** is used for automatically pushing content based on user-defined conditions. The XML file path should be specified relative to \<ApplicationPath>/conf.&#x20;

&#x20;**\<StreamName>** is used to match output stream names and supports the use of wildcard characters. **\<VariantNames>** can be used to select specific tracks. Multiple variants can be specified using commas (',').  The **\<Protocol>** supports rtmp, mpegts, and srt. You enter the destination address in the **\<Url>** field, where macros can also be used.

<pre class="language-xml"><code class="lang-xml">&#x3C;?xml version="1.0" encoding="UTF-8"?>
&#x3C;PushInfo>
  &#x3C;Push>
    &#x3C;!-- [Must] -->
    &#x3C;Enable>true&#x3C;/Enable>
    &#x3C;!-- [Must] -->
    &#x3C;StreamName>stream_a_*&#x3C;/StreamName>
    &#x3C;!-- [Optional] -->
    &#x3C;VariantNames>video_h264,audio_aac&#x3C;/VariantNames>
    &#x3C;!-- [Must] -->
    &#x3C;Protocol>rtmp&#x3C;/Protocol>
    &#x3C;!-- [Must] -->
    &#x3C;Url>rtmp://1.2.3.4:1935/app/${StreamName}&#x3C;/Url>
<strong>  &#x3C;/Push>  
</strong>  &#x3C;Push>
    &#x3C;!-- [Must] -->
    &#x3C;Enable>true&#x3C;/Enable>
    &#x3C;!-- [Must] -->
    &#x3C;StreamName>stream_b_*&#x3C;/StreamName>
    &#x3C;!-- [Optional] -->
    &#x3C;VariantNames>&#x3C;/VariantNames>
    &#x3C;!-- [Must] -->
    &#x3C;Protocol>srt&#x3C;/Protocol>
    &#x3C;!-- [Must] -->
    &#x3C;Url>srt://1.2.3.4:9999?streamid=srt%3A%2F%2F1.2.3.4%3A9999%2Fapp%2Fstream&#x3C;/Url>
  &#x3C;/Push>      
  &#x3C;Push>
    &#x3C;!-- [Must] -->
    &#x3C;Enable>false&#x3C;/Enable>
    &#x3C;!-- [Must] -->
    &#x3C;StreamName>stream_c_*&#x3C;/StreamName>
    &#x3C;!-- [Optional] -->
    &#x3C;VariantNames>&#x3C;/VariantNames>
    &#x3C;!-- [Must] -->
    &#x3C;Protocol>mpegts&#x3C;/Protocol>
    &#x3C;!-- [Must] -->
    &#x3C;Url>udp://1.2.3.4:2400&#x3C;/Url>
  &#x3C;/Push>    
&#x3C;/PushInfo>
</code></pre>



| Macro           | Description        |
| --------------- | ------------------ |
| ${Application}  | Application name   |
| ${SourceStream} | Source stream name |
| ${Stream}       | Output stream name |

## REST API

For control of push, use the REST API. SRT, RTMP, MPEGTS push can be requested based on the output stream name (specified in the JSON body), and you can selectively transfer all/some tracks. In addition, you must specify the URL and Stream Key of the external server to be transmitted. It can send multiple Pushes simultaneously for the same stream. If transmission is interrupted due to network or other problems, it automatically reconnects.

For how to use the API, please refer to the link below.

{% content-ref url="rest-api/v1/virtualhost/application/push.md" %}
[push.md](rest-api/v1/virtualhost/application/push.md)
{% endcontent-ref %}

