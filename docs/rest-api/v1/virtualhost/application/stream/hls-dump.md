# HLS Dump

The [LLHLS Dump feature](../../../../../streaming/low-latency-hls.md#dump) can be controlled with this API.

### Start Dump

> <mark style="color:red;">**POST**</mark>&#x20;
>
> http\[s]://{Host}/v1/vhosts/\<vhost\_name>/apps/\<app\_name>/streams/\<stream\_name>:startHlsDump
>
> <mark style="color:red;">**Body**</mark>
>
> ```xml
> {
>   "outputStreamName": "stream",
>   "id": "unique_dump_id",
>   "outputPath": "/tmp/",
>   "playlist" : ["llhls.m3u8", "abr.m3u8"],
>   "infoFile": "/home/abc/xxx/unique_info_file_name.info",
>   "userData": "access_key_id='AKIAXFOT7EWH3ZA4XXXX'"
> }
> ```
>
> **outputStreamName** (required)
>
>  The name of the output stream created with OutputProfile.
>
> **id** (required)
>
>  ID for this API request.
>
> **outputPath** (required)
>
>  Directory path to output. The directory must be writable by the OME process. OME will create the directory if it doesn't exist.
>
> **playlist** (optional)
>
>  Dump the master playlist set in outputPath. It must be entered in Json array format, and multiple playlists can be specified.
>
> **infoFile** (optional)
>
>  This is the name of the DB file in which the information of the dumped files is updated. If this value is not provided, no file is created. An error occurs if a file with the same name exists. (More details below)
>
> **userData** (optional)
>
>  If infoFile is specified, this data is written to infoFile. Does not work if infoFile is not specified.

### Stop Dump

> <mark style="color:red;">**POST**</mark>&#x20;
>
> http\[s]://{Host}/v1/vhosts/\<vhost\_name>/apps/\<app\_name>/streams/\<stream\_name>:stopHlsDump
>
> <mark style="color:red;">**Body**</mark>
>
> ```xml
> {
>   "outputStreamName": "stream",
>   "id": "dump_id"
> }
> ```
>
> **outputStreamName** (required)
>
>  The name of the output stream created with OutputProfile.
>
> **id** (optional)
>
>  This is the id passed when calling the startHlsDump API. If id is not passed, all dump in progress at outputStreamName is aborted.

### InfoFile&#x20;

The info file is continuously updated after the dump file is written. It is in XML format and is as follows. will continue to be added.

```
<HLSDumpInfo>
  <UserData>~~~</UserData>
  <Stream>/default/app/stream</Stream>
  <Status>Running | Completed | Error </Status>
  <Items>
    <Item>
      <Seq>0</Seq>
      <Time>~~~</Time>
      <File>~~~</File>
    </Item>
    ...
    <Item>
      <Seq>1</Seq>
      <Time>~~~</Time>
      <File>/tmp/abc/xxx/298182/chunklist_0_video_llhls.m3u8</File>
    </Item>
    ...
    <Item>
      <Seq>2</Seq>
      <Time>~~~</Time>
      <File>chunklist_0_video_llhls.m3u8</File>
    </Item>
  </Items>
</HLSDumpInfo>
```
