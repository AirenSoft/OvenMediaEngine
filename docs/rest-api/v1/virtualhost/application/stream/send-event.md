# Send Event

It allows you to insert events into streams. Right now events only support the ID3v2 format and only the LLHLS publisher handles it. Events delivered to LLHLS Publisher are inserted as emsg boxes within the m4s container.

> <mark style="color:red;">**POST**</mark>&#x20;
>
> http\[s]://{Host}/v1/vhosts/{vhost name}/apps/{app name}/streams/{stream name}:sendEvent
>
> <mark style="color:red;">**Body**</mark>
>
> ```
> {
>   "eventFormat": "id3v2",
>   "eventType": "video",
>   "events":[
>       {
>         "frameType": "TXXX",
>         "info": "AirenSoft",
>         "data": "OvenMediaEngine"
>       },
>       {
>         "frameType": "TIT2",
>         "data": "OvenMediaEngine 123"
>       }
>   ]
> }
> ```
>
> **eventFormat**\
>  Currently only `id3v2` is supported.
>
> **eventType** (Optional, Default : `event`)\
>   Select one of `event`, `video`, and `audio`. `event` inserts an event into every track. `video` inserts events only on tracks of type video. `audio` inserts events only on tracks of audio type.
>
> **events**\
>   It accepts only Json array format and can contain multiple events.
>
>  **frameType**\
>     Currently, only TXXX and T??? (Text Information Frames, e.g. TIT2) are supported.\
>  **info**\
>    This field is used only in TXXX and is entered in the Description field of TXXX.\
>  **data**\
>    If the frameType is TXXX, it is entered in the Value field, and if the frameType is "T???", it is entered in the Information field.

****
