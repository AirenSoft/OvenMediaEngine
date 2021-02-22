# Recording \(Beta\)

OvenMediaEngine can record live streams. You can start and stop recording the output stream through REST API. When the recording is complete, a recording information file is created together with the recorded file so that the user can perform various post-recording processing.

## Configuration

### File Publisher

To enable recording, add the `<FILE>` publisher to the configuration file as shown below. `<FilePath>` and `<InfoPath>` are required and used as default values. &lt;FilePath&gt; is the setting for the file path and file name. `<InfoPath>`is the setting for the path and name of the XML file that contains information about the recorded files. If there is no file path value among parameters when requesting recording through API, recording is performed with the set default value. This may be necessary if for security reasons you do not want to specify the file path when calling the API to avoid exposing the server's internal path. &lt;`<RootPath>` is an optional parameter. It is used when requesting with a relative path is required when requesting an API. also, it is applied to `<FilePath>` and `<InfoPath>` as in the example below.

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

<table>
  <thead>
    <tr>
      <th style="text-align:left">Macro</th>
      <th style="text-align:left">Description</th>
    </tr>
  </thead>
  <tbody>
    <tr>
      <td style="text-align:left">${TransactionId}</td>
      <td style="text-align:left">Unique ID for the recording transaction. It is automatically issued when
        recording starts by the user, and is released when recording is stopped.
        It can be saved as a file divided into several in one recording request.</td>
    </tr>
    <tr>
      <td style="text-align:left">${Id}</td>
      <td style="text-align:left">User-defined identification ID</td>
    </tr>
    <tr>
      <td style="text-align:left">${StartTime:YYYYMMDDhhmmss}</td>
      <td style="text-align:left">
        <p>Recording start time</p>
        <p>YYYY - Year</p>
        <p>MM - Month</p>
        <p>DD - Days</p>
        <p>hh : Hours (0~23)</p>
        <p>mm : Minutes (00~59)</p>
        <p>ss : Seconds (00~59)</p>
      </td>
    </tr>
    <tr>
      <td style="text-align:left">${EndTime:YYYYMMDDhhmmss}</td>
      <td style="text-align:left">
        <p>Recording end time</p>
        <p>YYYY - Year</p>
        <p>MM - Month</p>
        <p>DD - Days</p>
        <p>hh : Hours (0~23)</p>
        <p>mm : Minutes (00~59)</p>
        <p>ss : Seconds (00~59)</p>
      </td>
    </tr>
    <tr>
      <td style="text-align:left">${VirtualHost}</td>
      <td style="text-align:left">Virtual host name</td>
    </tr>
    <tr>
      <td style="text-align:left">${Application}</td>
      <td style="text-align:left">Application name</td>
    </tr>
    <tr>
      <td style="text-align:left">${SourceStream}</td>
      <td style="text-align:left">Source stream name</td>
    </tr>
    <tr>
      <td style="text-align:left">${Stream}</td>
      <td style="text-align:left">Output stream name</td>
    </tr>
    <tr>
      <td style="text-align:left">${Sequence}</td>
      <td style="text-align:left">Sequence value that increases when splitting a file in a single transaction</td>
    </tr>
  </tbody>
</table>

#### 

## Start & Stop Recording

For control of recording, use the REST API. Recording can be requested based on the output stream name \(specified in the JSON body\), and all/some tracks can be selectively recorded. And, it is possible to simultaneously record multiple files for the same stream. When recording is complete, an XML file is created at the path specified in InfoPath. For a sample of the recorded file information XML, refer to Appendix A.

For how to use the API, please refer to the link below.

{% page-ref page="rest-api/v1/virtualhost/application/recording.md" %}



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
    <recordTime>13816</recordTime>
    <sequence>0</sequence>
    <lastSequence>false</lastSequence>
    <createdTime>2020-12-04T12:53:51.455+0900</createdTime>
    <startTime>2020-12-04T12:53:51.612+0900</startTime>
    <finishTime>2020-12-04T12:54:05.473+0900</finishTime>
  </file>
  <file>
    <transactionId>bcUCyJeKuOGnsah3</transactionId>
    <id>CTS_ID001</id>
    <vhost>default</vhost>
    <app>app</app>
    <stream>stream_o</stream>
    <filePath><![CDATA[/home/dev/OvenMediaEngine/records/bcUCyJeKuOGnsah3_default_app_stream_o_20201204005408_20201204005412.ts]]></filePath>
    <recordBytes>2285797</recordBytes>
    <recordTime>3469</recordTime>
    <sequence>1</sequence>
    <lastSequence>false</lastSequence>
    <createdTime>2020-12-04T12:53:51.455+0900</createdTime>
    <startTime>2020-12-04T12:54:08.629+0900</startTime>
    <finishTime>2020-12-04T12:54:12.124+0900</finishTime>
  </file>
  <file>
    <transactionId>bcUCyJeKuOGnsah3</transactionId>
    <id>CTS_ID001</id>
    <vhost>default</vhost>
    <app>app</app>
    <stream>stream_o</stream>
    <filePath><![CDATA[/home/dev/OvenMediaEngine/records/bcUCyJeKuOGnsah3_default_app_stream_o_20201204005415_20201204005422.ts]]></filePath>
    <recordBytes>4544626</recordBytes>
    <recordTime>7009</recordTime>
    <sequence>2</sequence>
    <lastSequence>true</lastSequence>
    <createdTime>2020-12-04T12:53:51.455+0900</createdTime>
    <startTime>2020-12-04T12:54:15.136+0900</startTime>
    <finishTime>2020-12-04T12:54:22.144+0900</finishTime>
  </file>
</files>
```



