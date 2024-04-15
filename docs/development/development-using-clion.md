# Using CLion on Ubuntu

To develop OvenMediaEngine using CLion, using the following steps:

1. Clone the Git repository
2. Run the `misc/prerequisites.sh` to install dependencies, for more information, [check here](../getting-started/README.md#installing-dependencies)
3. Open the `src` folder using CLion
4. Go to the `Run` dropdown on the right top, you should find a lot of tasks available to run
5. Run the `all` task, it opends the `Edit Configuration` modal
6. It shows an error, but click on `Run` anyway, and later confirm the run again
7. CLion should show logs of compilation, after it finishes, you should find an executable in `src/bin/DEBUG`
8. Add the [server.xml](#serverxml-example) config file to `src/bin/DEBUG/config`
9. Click on the three dots on the top right, closed to the `Debug` icon
10. Choose edit, it opens the `Run/Debug Configuration`
11. Click on the `+` sign to add a new configuration, choose `Native Application`
12. Set the `Name` as run, `Target` all, `Executable` the binary on the path `src/bin/DEBUG/OvenMediaEngine` 
13. Run the run task

To test the stream, run the following FFMPEG command:

```ffmpeg -re -f lavfi -i testsrc=size=1280x720:rate=30,format=yuv420p -f lavfi -i sine=frequency=440:sample_rate=44100 -c:v libx264 -preset medium -crf 23 -c:a aac -b:a 128k -shortest -f mpegts udp://127.0.0.1:4000```

And you can access the stream here: http://localhost:3333/app/live/playlist.m3u8

## server.xml example

```
<?xml version="1.0" encoding="UTF-8"?>

<Server version="8">
    <Name>OvenMediaEngine</Name>
    <Type>origin</Type>
    <IP>*</IP>
    <PrivacyProtection>false</PrivacyProtection>

    <StunServer>stun.l.google.com:19302</StunServer>

    <Modules>
        <HTTP2>
            <Enable>true</Enable>
        </HTTP2>

        <LLHLS>
            <Enable>true</Enable>
        </LLHLS>
    </Modules>

    <Bind>
        <Managers>
            <API>
                <Port>8081</Port>
                <TLSPort>8082</TLSPort>
                <WorkerCount>1</WorkerCount>
            </API>
        </Managers>
        <Providers>
            <MPEGTS>
                <Port>4000/udp</Port>
            </MPEGTS>
        </Providers>

        <Publishers>
            <LLHLS>
                <Port>3333</Port>
                <TLSPort>3334</TLSPort>
                <WorkerCount>1</WorkerCount>
            </LLHLS>
        </Publishers>
    </Bind>
    <Managers>
        <Host>
            <Names>
                <Name>*</Name>
            </Names>
        </Host>
        <API>
            <AccessToken>admin:admin</AccessToken>
            <CrossDomains>
                <Url>*</Url>
                <Url>http://localhost</Url>
            </CrossDomains>
        </API>
    </Managers>

    <VirtualHosts>
        <VirtualHost>
            <Name>default</Name>
            <Distribution>ovenmediaengine.com</Distribution>

            <Host>
                <Names>
                    <Name>*</Name>
                </Names>
            </Host>
            <Applications>
                <Application>
                    <Name>app</Name>
                    <Type>live</Type>
                    <Providers>
                        <MPEGTS>
                            <StreamMap>
                                <Stream>
                                    <Name>live</Name>
                                    <Port>4000</Port>
                                </Stream>
                            </StreamMap>
                        </MPEGTS>
                    </Providers>
                    <OutputProfiles>
                        <HardwareAcceleration>false</HardwareAcceleration>
                        <OutputProfile>
                            <Name>bypass_stream</Name>
                            <OutputStreamName>${OriginStreamName}</OutputStreamName>
                            <Playlist>
                                <Name>For LLHLS</Name>
                                <FileName>playlist</FileName>
                                <Rendition>
                                    <Name>FHD</Name>
                                    <Video>video_720</Video>
                                    <Audio>bypass_audio</Audio>
                                </Rendition>
                            </Playlist>
                            <Encodes>
                                <Audio>
                                    <Name>bypass_audio</Name>
                                    <Codec>aac</Codec>
                                    <Bitrate>128000</Bitrate>
                                    <Samplerate>48000</Samplerate>
                                    <Channel>2</Channel>
                                </Audio>
                                <Video>
                                    <Name>video_720</Name>
                                    <Codec>h264</Codec>
                                    <Bitrate>2024000</Bitrate>
                                    <Framerate>30</Framerate>
                                    <Height>480</Height>
                                    <Width>640</Width>
                                    <Preset>faster</Preset>
                                </Video>
                            </Encodes>
                        </OutputProfile>
                    </OutputProfiles>
                    <Publishers>
                        <AppWorkerCount>10</AppWorkerCount>
                        <StreamWorkerCount>10</StreamWorkerCount>
                        <LLHLS>
                            <ChunkDuration>0.2</ChunkDuration>
                            <SegmentDuration>4</SegmentDuration>
                            <SegmentCount>5</SegmentCount>
                            <Dumps>
                                <Dump>
                                    <Enable>false</Enable>
                                    <TargetStreamName>live</TargetStreamName>

                                    <Playlists>
                                        <Playlist>playlist.m3u8</Playlist>
                                    </Playlists>
                                    <OutputPath>
                                        /tmp/vod/html/${VHostName}_${AppName}_${StreamName}/${YYYY}_${MM}_${DD}_${hh}_${mm}_${ss}
                                    </OutputPath>
                                </Dump>
                            </Dumps>
                            <CrossDomains>
                                <Url>*</Url>
                            </CrossDomains>
                        </LLHLS>
                    </Publishers>
                </Application>
            </Applications>
        </VirtualHost>
    </VirtualHosts>
</Server>
```



