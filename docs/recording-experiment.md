# Recording (Beta)

OvenMediaEngine can record live streams. You can start and stop recording the output stream through REST API. When the recording is complete, a recording information file is created together with the recorded file so that the user can perform various post-recording processing.

## Configuration

### File Publisher

To enable recording, add the `<FILE>` publisher to the configuration file as shown below. `<FilePath>` and `<InfoPath>` are required and used as default values. \<FilePath> is the setting for the file path and file name. `<InfoPath>`is the setting for the path and name of the XML file that contains information about the recorded files. If there is no file path value among parameters when requesting recording through API, recording is performed with the set default value. This may be necessary if for security reasons you do not want to specify the file path when calling the API to avoid exposing the server's internal path. <`<RootPath>` is an optional parameter. It is used when requesting with a relative path is required when requesting an API. also, it is applied to `<FilePath>` and `<InfoPath>` as in the example below.

You must specify `.ts` or `.mp4` at the end of the FilePath string to select a container for the recording file. We recommend using .ts unless you have a special case. This is because vp8 and opus codecs are not recorded due to container limitations if you choose .mp4.

```markup
<Publishers>
	<FILE>
		<RootPath>/mnt/shared_volumes</RootPath>
		<FilePath>/${VirtualHost}/${Application}/${Stream}/${StartTime:YYYYMMDDhhmmss}_${EndTime:YYYYMMDDhhmmss}.ts</FilePath>
		<InfoPath>/${VirtualHost}/${Application}/${Stream}.xml</InfoPath>
	</FILE>
</Publishers>
```

Various macro values are supported for file paths and names as shown below.

#### Macro Definition

| Macro                       | Description                                                                                                                                                                                                             |
| --------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| ${TransactionId}            | Unique ID for the recording transaction. It is automatically created when recording starts. and is released when recording is stopped. In case of split recording, it is distinguished that it is the same transaction. |
| ${Id}                       | User-defined identification ID                                                                                                                                                                                          |
| ${StartTime:YYYYMMDDhhmmss} | <p>Recording start time</p><p>YYYY - Year</p><p>MM - Month</p><p>DD - Days</p><p>hh : Hours (0~23) </p><p>mm : Minutes (00~59) </p><p>ss : Seconds (00~59)</p>                                                          |
| ${EndTime:YYYYMMDDhhmmss}   | <p>Recording end time</p><p>YYYY - Year</p><p>MM - Month</p><p>DD - Days</p><p>hh : Hours (0~23) </p><p>mm : Minutes (00~59) </p><p>ss : Seconds (00~59)</p>                                                            |
| ${VirtualHost}              | Virtual host name                                                                                                                                                                                                       |
| ${Application}              | Application name                                                                                                                                                                                                        |
| ${SourceStream}             | Source stream name                                                                                                                                                                                                      |
| ${Stream}                   | Output stream name                                                                                                                                                                                                      |
| ${Sequence}                 | Sequence value that increases when splitting a file in a single transaction                                                                                                                                             |

####

## Start & Stop Recording

For control of recording, use the REST API. Recording can be requested based on the output stream name (specified in the JSON body), and all/some tracks can be selectively recorded. And, it is possible to simultaneously record multiple files for the same stream. When recording is complete, an XML file is created at the path specified in InfoPath. For a sample of the recorded file information XML, refer to Appendix A.

For how to use the API, please refer to the link below.

{% content-ref url="rest-api/v1/virtualhost/application/recording.md" %}
[recording.md](rest-api/v1/virtualhost/application/recording.md)
{% endcontent-ref %}

### Split Recording

Split recording methods provide **interval** and **schedule**.  The interval method splits files based on the accumulated recording time. The Schedule method then splits files according to scheduling options based on system time. The scheduling option is the same as the pattern used in crontab. However, only three options are used: seconds/minutes/hour.

{% hint style="info" %}
**interval** and **schedule** methods cannot be used simultaneously.&#x20;
{% endhint %}

## Appendix A. Recorded File Information Specification

The following is a sample of an XML file that expresses information on a recorded file.

```markup
<?xml version="1.0" encoding="utf-8"?>
<files>
  <file>
    <transactionId>bcUCyJeKuOGnsah3</transactionId>
    <id>CTS_ID001</id>
    <vhost>default</vhost>
    <app>app</app>
    <stream>stream_o</stream>
    <filePath><![CDATA[/home/dev/OvenMediaEngine/records/bcUCyJeKuOGnsah3_default_app_stream_o_20201204005351_20201204005405.ts]]></filePath>
    <recordBytes>8774737</recordBytes>
    <recordTime>60011</recordTime>
    <sequence>0</sequence>
    <interval>60000</interval>
    <lastSequence>true</lastSequence>
    <createdTime>2020-12-04T12:53:51.455+0900</createdTime>
    <startTime>2020-12-04T12:53:51.612+0900</startTime>
    <finishTime>2020-12-04T12:54:51.473+0900</finishTime>
  </file>
  <file>
    <transactionId>bcUCyJeKuOGnsah3</transactionId>
    <id>CTS_ID001</id>
    <vhost>default</vhost>
    <app>app</app>
    <stream>stream_o</stream>
    <filePath><![CDATA[/home/dev/OvenMediaEngine/records/bcUCyJeKuOGnsah3_default_app_stream_o_20201204005408_20201204005412.ts]]></filePath>
    <recordBytes>2285797</recordBytes>
    <recordTime>60012</recordTime>
    <sequence>0</sequence>
    <schedule>0 */1 *</schedule>
    <lastSequence>false</lastSequence>
    <createdTime>2020-12-04T12:53:00.000+0900</createdTime>
    <startTime>2020-12-04T12:53:00.000+0900</startTime>
    <finishTime>2020-12-04T12:54:00.000+0900</finishTime>
  </file>
  <file>
    <transactionId>bcUCyJeKuOGnsah3</transactionId>
    <id>CTS_ID001</id>
    <vhost>default</vhost>
    <app>app</app>
    <stream>stream_o</stream>
    <filePath><![CDATA[/home/dev/OvenMediaEngine/records/bcUCyJeKuOGnsah3_default_app_stream_o_20201204005415_20201204005422.ts]]></filePath>
    <recordBytes>4544626</recordBytes>
    <recordTime>60000</recordTime>
    <sequence>1</sequence>
    <schedule>0 */1 *</schedule>
    <lastSequence>true</lastSequence>
    <createdTime>2020-12-04T12:54:00.000+0900</createdTime>
    <startTime>2020-12-04T12:54:00.000+0900</startTime>
    <finishTime>2020-12-04T12:55:00.000+0900</finishTime>
  </file>
</files>
```

