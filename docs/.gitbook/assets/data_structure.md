# Primitive Data Types
<table border="1">
	<thead>
		<tr>
			<th>Type</th>
			<th>Description</th>
			<th>Examples</th>
		</tr>
	</thead>
	<tbody>
		<tr>
			<td>Short</td>
			<td>16bits integer</td>
			<td><code>12345</code></td>
		</tr>
		<tr>
			<td>Int</td>
			<td>32bits integer</td>
			<td><code>1234941932</code></td>
		</tr>
		<tr>
			<td>Long</td>
			<td>64bits integer</td>
			<td><code>391859818923919232311</code></td>
		</tr>
		<tr>
			<td>Float</td>
			<td>64bits real number</td>
			<td><code>3.5483</code></td>
		</tr>
		<tr>
			<td>String</td>
			<td>A string</td>
			<td><code>"Hello"</code></td>
		</tr>
		<tr>
			<td>Bool</td>
			<td>true/false</td>
			<td><code>true</code></td>
		</tr>
	</tbody>
</table>

# Containers/Notations

## Enum<```Type```> (String)
- An enum value named ```Type```
- Examples
  - ```"value1"```
  - ```"value2"```

## List<```Type```>
- An ordered list of ```Type```
- Examples
  - ```[ Type, Type, ... ]```

## Map<```KeyType```, ```ValueType```>
- An object consisting of ```Key```-```Value``` pairs
- Examples
  - ```{ Key1: Value1, Key2: Value2 }```

# Common Data Types

## ```Timestamp```
- A timestamp in ISO8601 format
- Examples
  - ```"2020-10-30T11:00:00.000+09:00"```

## ```TimeInterval``` (Long)
- A time interval (unit: ms)
- Examples
  - ```349820```

## ```IP``` (String)
- IP address
- Examples
  - ```"127.0.0.1"```

## ```RangedPort``` (String)
- Port numbers with range - it can contain multiple ports and protocols
  - ```start_port[-end_port][,start_port[-end_port][,start_port[-end_port]...]][/protocol]```
- Examples
  - ```"40000-40005/tcp"```
  - ```"40000-40005"```
  - ```"40000-40005,10000,20000/tcp"```

## ```Port``` (String)
- A port number
  - ```start_port[/protocol]```
- Examples
  - ```"1935/tcp"```
  - ```"1935"```

## Enum<```ApplicationType```>
- Application type
- Examples
  - ```"live"```
  - ```"vod"```

## Enum<```Codec```>
- Codecs
- Examples
  - ```"h264"```
  - ```"h265"```
  - ```"vp8"```
  - ```"opus"```
  - ```"aac"```

## Enum<```StreamSourceType```>
- A type of input stream
- Examples
  - ```"Ovt"```
  - ```"Rtmp"```
  - ```"Rtspc"```
  - ```"RtspPull"```
  - ```"MpegTs"```

## Enum<```MediaType```>
- type
- Examples
  - ```"video"```
  - ```"audio"```

## Enum<```SessionState```>
- A state of the session
- Examples
  - ```"Ready"```
  - ```"Started"```
  - ```"Stopping"```
  - ```"Stopped"```
  - ```"Error"```

## Enum<```AudioLayout```>
- Audio layout
- Examples
  - ```"stereo"```
  - ```"mono"```

# Data Types

Type name	Fields	Remarks	Example
	Type	Name	Optional	POST	GET	PUT	Description		

## VirtualHost
<table>
	<thead>
		<tr>
			<th>Type</th>
			<th>Name</th>
			<th>Optional</th>
			<th>Description</th>
			<th>Examples</th>
		</tr>
	</thead>
	<tbody>
		<tr>
			<td>String</td>
			<td>name</td>
			<td>N</td>
			<td>A name of Virtual Host</td>
			<td><code>"default"</code></td>
		</tr>
		<tr>
			<td><a href="data-types#host">Host</a></td>
			<td>host</td>
			<td>Y</td>
			<td>A information of Host</td>
			<td><code>Host</code></td>
		</tr>
	</tbody>
</table>

## Host

- String	name	N		v		Virtual Host 이름		"default"
	Host	host	Y		v		Host 정보		Host
	SignedPolicy	signedPolicy	Y		v		Signed policy 정보		SignedPolicy
	SignedToken	signedToken	Y		v		Signed token 정보		SignedUrl
	List<OriginMap>	originMaps	Y		v		origin map 목록		[OriginMap, OriginMap, ...]

Host	List<String>	names	N		v		Host 이름들		["airensoft.com", "*.test.com", ...]
	Tls	tls	Y		v		TLS 정보		Tls
Tls	String	certPath	N		v		인증서 경로		"a.crt"
	String	keyPath	N		v		키 파일 경로		"a.key"
	String	chainCertPath	Y		v		Chain 인증서 경로		"c.crt"
SignedPolicy	String	policyQueryKey	N		v				
	String	signatureQueryKey	N		v				
	String	secretKey	N		v				
SignedToken	String	cryptoKey	N		v				
	String	queryStringKey	N		v				
OriginMap	String	location	N		v		mapping할 패턴		"/"
	Pass	pass	N		v		패턴에 일치할 경우, Origin에 요청할 정보		Pass
Pass	String	scheme	N		v		Provider를 선별하기 위한 Scheme		"ovt"
	List<String>	urls	N		v		Provider에서 Pull 할 주소 목록		["origin:9000", "origin2:9000", ...]
Application	String	name	N	v	v		App 이름	PUT으로 변경 불가	"app"
	Bool	dynamic	N		v		App이 동적으로 생성되었는지 여부	Server.xml에 명시하지 않은 상태에서, PullStream()하면 동적으로 생성됨	true
	Enum<ApplicationType>	type	N	v	v		App 타입	PUT으로 변경 불가
"vod"는 지원 안함	"live"
	Providers	providers	Y	v	v	v	Provider 목록		Providers
	Publishers	publishers	Y	v	v	v	Publisher 목록		Publishers
	List<OutputProfile>	outputProfiles	Y	v			OutputProfile 정보들

생략 시, 다음과 같이 생성됨

- Encode #1 
- Video: bypass 
- Audio: bypass
- Encode #2 
- Audio: OPUS	HttpRequest에만 사용됨 - App 생성과 동시에 OutputProfile을 설정할 때에만 사용	[OutputProfile, OutputProfile, ...]
Providers	RtmpProvider	rtmp	Y	v	v	v		비활성화 되어 있을 경우 null이 반환됨	RtmpProvider
	RtspPullProvider	rtspPull	Y	v	v	v		비활성화 되어 있을 경우 null이 반환됨	RtspPullProvider
	RtspProvider	rtsp	Y	v	v	v		비활성화 되어 있을 경우 null이 반환됨	RtspProvider
	OvtProvider	ovt	Y	v	v	v		비활성화 되어 있을 경우 null이 반환됨	OvtProvider
	MpegtsProvider	mpegts	Y	v	v	v		비활성화 되어 있을 경우 null이 반환됨	MpegtsProvider
RtmpProvider	(Empty object)	-	-				-	향후에 option이 추가될 수 있어 일단 선언만 해놓음	
RtspPullProvider	(Empty object)	-	-				-	향후에 option이 추가될 수 있어 일단 선언만 해놓음	
RtspProvider	(Empty object)	-	-				-	향후에 option이 추가될 수 있어 일단 선언만 해놓음	
OvtProvider	(Empty object)	-	-				-	향후에 option이 추가될 수 있어 일단 선언만 해놓음	
MpegtsProvider	List<MpegtsStream>	streams	Y	v	v	v	MPEG-TS Stream map		[MpegtsStream, MpegtsStream, ...]
MpegtsStream	String	name	N	v	v		MPEG-TS Stream이 들어왔을 때, 생성되는 이름	port로 데이터가 들어오면 생성할 stream 이름	"stream"
	RangedPort	port	Y	v	v	v	MPEG-TS Port	데이터를 수신할 포트	"4000-4001/udp"
Publishers	Int	threadCount	N	v	v	v	thread 개수		4
	RtmpPushPublisher	rtmpPush	Y	v	v	v		비활성화 되어 있을 경우 null이 반환됨	RtmpPushPublisher
	HlsPublisher	hls	Y	v	v	v		비활성화 되어 있을 경우 null이 반환됨	HlsPublisher
	DashPublisher	dash	Y	v	v	v		비활성화 되어 있을 경우 null이 반환됨	DashPublisher
	LlDashPublisher	llDash	Y	v	v	v		비활성화 되어 있을 경우 null이 반환됨	LlDashPublisher
	WebrtcPublisher	webrtc	Y	v	v	v		비활성화 되어 있을 경우 null이 반환됨	WebrtcPublisher
	OvtPublisher	ovt	Y	v	v	v		비활성화 되어 있을 경우 null이 반환됨	OvtPublisher
	FilePublisher	file	Y	v	v	v		비활성화 되어 있을 경우 null이 반환됨	FilePublisher
	ThumbnailPublisher	thumbnail	Y	v	v	v		비활성화 되어 있을 경우 null이 반환됨	ThumbnailPublisher
RtmpPushPublisher	(Empty object)	-	-				-	향후에 option이 추가될 수 있어 일단 선언만 해놓음	
HlsPublisher	Int	segmentCount	N	v	v		playlist내 segment 개수		3
	Int	segmentDuration	N	v	v		segment 길이		4
	List<String>	crossDomains	Y	v	v	v	cross domain 허용할 URL 목록		["*"]
DashPublisher	Int	segmentCount	N	v	v		playlist내 segment 개수		3
	Int	segmentDuration	N	v	v		segment 길이		4
	List<String>	crossDomains	Y	v	v	v	cross domain 허용할 URL 목록		["*"]
LlDashPublisher	Int	segmentDuration	N	v	v		segment 길이		3
	List<String>	crossDomains	Y	v	v	v	cross domain 허용할 URL 목록		["*"]
WebrtcPublisher	TimeInterval	timeout	Y	v	v	v	ICE timeout 시간		30
OvtPublisher	(Empty object)	-	-				-	향후에 option이 추가될 수 있어 일단 선언만 해놓음	
FilePublisher	String	filePath	Y	v	v	v	녹화 파일 저장 경로	[매크로 정의는 아래 링크 참조]
https://airensoft.atlassian.net/wiki/spaces/OME/pages/825065477/OME-1189+File+Publisher
"/tmp/${StartTime:YYYYMMDDhhmmss}_${Stream}.mp4"
	String	fileInfoPath	Y	v	v	v	녹화 파일 정보 저장 경로		"/tmp/${StartTime:YYYYMMDDhhmmss}_${Stream}.xml"
ThumbnailPublisher	List<String>	crossDomains	Y	v	v	v	cross domain 허용할 URL 목록		["*"]
OutputProfile	String	name	N	v	v		Output profile 이름		"bypass_stream"
	String	outputStreamName	N	v	v	v	Output stream 이름		"${OriginStreamName}"
	Encodes	encodes	Y	v	v	v			[Encodes, Encodes, ...]
Encodes	List<Video>	videos	Y	v	v	v			[Video, Video, ...]
	List<Audio>	audios	Y	v	v	v			[Audio, Audio, ...]
	List<Image>	images	Y	v	v	v			[Image, Image, ...]
Video	Bool	bypass	Y	v	v	v			true
	Enum<Codec>	codec	C-Y	v	v	v	Video 코덱	bypass == false 일 때 필수	"h264"
	Int	width	C-Y	v	v	v			1280
	Int	height	C-Y	v	v	v			720
	String	bitrate	C-Y	v	v	v	bitrate - K 또는 M 접미어 사용 가능 (100K, 3M)		"3000000"
"2.5M"
	Float	framerate	C-Y	v	v	v			29.997
Audio	Bool	bypass	Y	v	v	v			true
	Enum<Codec>	codec	C-Y	v	v	v	Audio 코덱	bypass == false 일 때 필수	"opus"
	Int	samplerate	C-Y	v	v	v			48000
	Int	channel	C-Y	v	v	v			2
	String	bitrate	C-Y	v	v	v	bitrate - K 또는 M 접미어 사용 가능 (128K, 0.1M)		"128000"
"128K"
Image	Enum<Codec>	codec	N	v	v	v			"jpeg" | "png"
	Int	width	C-Y	v	v	v		width, height 가 모두 설정이 안된 경우 = 원본 사이즈
width만 설정된 경우 = height는 원본 비율에 맞추어 자동
height만 설정된 경우 = width는 원본 비율에 맞추어 자동
width, height 모두 설정된 경우 = 설정된 사이즈	854
	Int	height	C-Y	v	v	v			480
	Float	framerate	N	v	v	v	이미지 생성 주기		1
Stream	String	name	N		v		Stream 이름		"stream"
	InputStream	input	N		v		입력 stream 정보		InputStream
	List<OutputStream>	outputs	N		v		출력 stream 정보		[OutputStream, OutputStream, ...]
NewStream	String	name	N	v			생성할 Stream 이름		"stream"
	PullStream	pull	Y	v			pull		PullStream
	MpegtsStream	mpegts	Y	v			prestream 생성		MpegtsStream
PullStream	String	url	N	v			Pull 할 URL		"rtsp://1200000222.wstl.vcase2.myskcdn.com/1200000222_l/_definst_/1490012175?cid=1490012175&pid=1991749339"
InputStream	String	agent	Y		v		송출하고 있는 Agent 이름		"OBS 12.0.4"
	String	from	N		v		Stream이 생성된 원본 URI	Signalling과 data channel이 분리되어 있을 경우, Signalling 기준
- OBS로 송출 할 경우 "tcp://192.168.0.200:33399"와 같이 접속된 remote 정보
- pull stream일 경우 "rtsp://rtsp_server:12345/app/stream"과 같이 pull한 URL	"tcp://192.168.0.200:33399"
	String	to	Y		v		Stream을 수신하고 있는 OME의 정보	Signalling과 data channel이 분리되어 있을 경우, Signalling 기준
- from이 file:// 일 경우 제공 안함	"rtmp://dev.airensoft.com:1935"
	List<Track>	tracks	N		v		Input stream에 포함된 track 목록		[Track, Track, ...]
	Timestamp	createdTime	N		v		생성시각		"2020-10-30T11:00:00+09:00"
OutputStream	String	name	N		v		OutputProfile에 의해 생성된 Output stream 이름		"stream_o"
	List<Track>	tracks	N		v		Output stream에 포함된 track 목록		[Track, Track, ...]
Track	Enum<MediaType>	type	Y		v		Media type		"video"
	Video	video	C-Y		v		video 인코딩 설정 정보	type == "video" 일 때 필수	Video
	Audio	audio	C-Y		v		audio 인코딩 설정 정보	type == "audio" 일 때 필수	Audio
VideoTrack	(Extends Video)	-	-				Video 값들 모두 포함됨		
	Timebase	timebase	Y		v		timebase		Timebase
Timebase	Int	num	N		v		numerator		1
	Int	den	N		v		denominator		90000
AudioTrack	(Extends Audio)	-	-				Audio 값들 모두 포함됨		true
	Timebase	timebase	Y		v		timebase		Timebase
Record	String	id	Y	v	v	v	식별 아이디	입력 없음면 자동 발긊	
	OutputStream	streams	N	v	v	v	출력 스트림의 트랙명과 트랙아이디 조합		
	Enum<SessionState>	state	N		v		녹화 상태		
	String	filePath	N		v		녹화 파일 경로		
	String	fileInfoPath	N		v		녹화 정보 파일 경로		
	String	recordedBytes	N		v		녹화 용량		
	Int	recordedTime	N		v		녹화 시간		
	Timestamp	startTime	N		v		시작 시간		
	Timestamp	finishTime	N		v		종료 시간		
	Int	bitrate	N		v		평균 비트레이트		
Push	String	id	N	v	v	v	식별 아이디	입력 없음면 자동 발긊	
	OutputStream	stream	Y	v	v	v	출력 스트림의 트랙명과 트랙아이디 조합		
	Enum<StreamSourceType>	protocol	Y	v	v	v	송출 프로토콜		
	String	url	Y	v	v	v	목적지 URL		
	String	streamKey	C-Y	v	v	v	목적지 스트림키	protocol == "rtmp" 일 때 필수	
	Enum<SessionState>	state	N		v		송출 상태		
	Int	sentBytes	N		v		송출 용량		
	Int	sentPackerts	N		v		송출 패킷 수		
	Int	snetErrorBytes	N		v		송출 에러 용량		
	Int	sentErrorPackets	N		v		송출 에러 패킷 수 		
	Int	reconnect	N		v		재접속 수		
	Timestamp	startTime	N		v		시작 시간		
	Timestamp	finishTime	N		v		종료 시간		
	Int	bitrate	N		v		평균 비트레이트		
CommonMetrics	Timestamp	createdTime	N		v		생성된 시각		"2020-10-30T11:00:00+09:00"
	Timestamp	lastUpdatedTime	N		v		마지막 업데이트 된 시각		"2020-10-30T11:00:00+09:00"
	Long	totalBytesIn	N		v		수신한 데이터 크기 (단위: bytes)		3109481213
	Long	totalBytesOut	N		v		송신한 데이터 크기 (단위: bytes)		1230874123
	Int	totalConnections	N		v		현재 총 접속 수		10
	Int	maxTotalConnections	N		v		최대 동접 수		293
	Timestamp	maxTotalConnectionTime	N		v		최대 동접 수가 갱신된 시각		"2020-10-30T11:00:00+09:00"
	Timestamp	lastRecvTime	N		v		마지막으로 데이터를 수신한 시각		"2020-10-30T11:00:00+09:00"
	Timestamp	lastSentTime	N		v		마지막으로 데이터를 송신한 시각		"2020-10-30T11:00:00+09:00"
StreamMetrics	(Extends CommonMetrics)	-	-		v		CommonMetrics 값들 모두 포함됨		
	TimeInterval	requestTimeToOrigin	Y		v		Origin에 접속되기까지 걸린 시간		1000
	TimeInterval	responseTimeFromOrigin	Y		v		Origin에서 응답 주기까지 걸린 시간		10000
