# Push Publishing \(Beta\)

OvenMediaEngine supports **Push Publishing** function that can retransmit live streams to other systems. The protocol supported for retransmission uses RTMP. Because, most services and products support this protocol.  also, one output stream can be transmitted to multiple destinations at the same time.  You can start and stop pushing the output stream through REST API.  Note that the only codecs that can be retransmitted in RTMP protocol are H264 and AAC.

## Configuration

### RTMPPush Publisher

To use RTMP Push Publishing, you need to declare the `<RTMPPush>` publisher in the configuration.  There are no other detailed options.

```markup
<Applications>
  <Application>
     ...
    <Publishers>
      ... 
  	  <RTMPPush>
    	</RTMPPush>
    </Publishers>	
  </Application>
</Applications>
```

{% hint style="info" %}
Only H264 and AAC are supported codecs. 
{% endhint %}

### Start & Stop Push

For control of push, use the REST API. RTMP push can be requested based on the output stream name \(specified in the JSON body\), and you can selectively transfer all/some tracks.  In addition, you must specify the URL and Stream Key of the external server to be transmitted.  It can send multiple Pushes simultaneously for the same stream. If transmission is interrupted due to network or other problems, it automatically reconnects. 

For how to use the API, please refer to the link below.

{% page-ref page="rest-api/v1/virtualhost/application/push.md" %}





