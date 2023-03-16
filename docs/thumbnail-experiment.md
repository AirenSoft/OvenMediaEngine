# Thumbnail

OvenMediaEngine can generate thumbnails from live streams. This allows you to organize a broadcast list on your website or monitor multiple streams at the same time.

## Configuration

### Bind

Thumbnails are published via HTTP(s). Set the port for thumbnails as follows. Thumbnail publisher can use the same port number as HLS and DASH.

```markup
<Bind>
    <Publishers>
      ...
        <Thumbnail>
            <Port>20080</Port>
            <!-- If you need TLS support, please uncomment below:
            <TLSPort>20081</TLSPort>
            -->
        </Thumbnail>
    </Publishers>
</Bind>
```

### Encoding

In order to publish thumbnails, an encoding profile for thumbnails must be set. JPG and PNG are supported as codec. And framerate and resolution can be adjusted.  Framerate is the number of thumbnails extracted per second. We recommend 1 as the thumbnail framerate. Thumbnail encoding uses a lot of resources. Therefore, if you increase this value excessively, it can cause a failure due to excessive use of system resources. The resolution can be set as desired by the user, and if the ratio is different from the input image, it is stretched. We plan to support various ratio modes in the future.

```markup
<OutputProfiles>
    <OutputProfile>
        <Name>default_stream</Name>
        <OutputStreamName>${OriginStreamName}_preview</OutputStreamName>
        <Encodes>
            <Image>
                <Codec>jpeg</Codec>
                <Framerate>1</Framerate>
                <Width>1280</Width>
                <Height>720</Height>
            </Image>
            <Image>
                <Codec>png</Codec>
                <Framerate>1</Framerate>
                <Width>1280</Width>
                <Height>720</Height>
            </Image>
        </Encodes>
    </OutputProfile>
</OutputProfiles>
```

### Publisher

Declaring a thumbnail publisher. Cross-domain settings are available as a detailed option.

```markup
<Publishers>
    ...
    <Thumbnail>
        <CrossDomains>
            <Url>*</Url>
        </CrossDomains>	
    </Thumbnail>
</Publishers>
```

## Get thumbnails

When the setting is made for the thumbnail and the stream is input, you can view the thumbnail through the following URL.

| Method | URL Pattern                                                                             |
| ------ | --------------------------------------------------------------------------------------- |
| GET    | http(s)://\<ome\_host_>:\<port>/\<app\_name>/\<output\_stream\_name>/thumb.\<jpg\|png>_ |
