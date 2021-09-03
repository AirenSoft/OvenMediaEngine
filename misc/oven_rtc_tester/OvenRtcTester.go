package main

import (
	"encoding/json"
	"flag"
	"fmt"
	"io"
	"math"
	"os"
	"os/signal"
	"sync"
	"time"

	"github.com/gorilla/websocket"
	"github.com/pion/webrtc/v3"
)

const jitterThreshold = 1000   // ms
const delayReportPeriod = 5000 // ms

func main() {
	url := flag.String("url", "", "[Required] OvenMediaEngine's webrtc streaming URL")
	numberOfClient := flag.Int("n", 1, "[Optional] Number of client")

	flag.Usage = func() {
		fmt.Printf("Usage: %s [-n number_of_clients] OvenMediaEngine_URL\n", os.Args[0])
		flag.PrintDefaults()
	}

	flag.Parse()

	// URL is mandatory
	if len(*url) == 0 {
		flag.CommandLine.Usage()
		return
	}

	var clients = make([]*omeClient, 0, *numberOfClient)
	for i := 0; i < *numberOfClient; i++ {
		client := omeClient{}

		client.name = fmt.Sprintf("client_%d", i)
		err := client.run(*url)
		if err != nil {
			fmt.Printf("%s failed to run (reason - %s)\n", client.name, err)
			return
		}
		fmt.Printf("%s has started\n", client.name)
		defer client.stop()

		clients = append(clients, &client)
	}

	closed := make(chan os.Signal, 1)
	signal.Notify(closed, os.Interrupt)
	<-closed

	fmt.Println("***************************")
	fmt.Println("Reports")
	fmt.Println("***************************")
	for _, client := range clients {
		if client == nil {
			continue
		}

		client.report()
	}
}

type signalingClient struct {
	url    string
	socket *websocket.Conn
}

type sessionStat struct {
	startTime time.Time

	maxFPS float32
	minFPS float32

	maxBPS int64
	minBPS int64

	totalBytes       int64
	totalVideoFrames int64
	totalRtpPackets  int64
	packetLoss       int64

	videoRtpTimestampElapsedMSec int64
	audioRtpTimestampElapsedMSec int64
}

type omeClient struct {
	name string

	sc             *signalingClient
	peerConnection *webrtc.PeerConnection

	videoAvailable bool
	videoMimeType  string
	audioAvailable bool
	audioMimeType  string

	stat sessionStat

	once  sync.Once
	mutex sync.Mutex
}

func (sc *signalingClient) connect(url string) error {

	sc.url = url

	var err error
	sc.socket, _, err = websocket.DefaultDialer.Dial(url, nil)
	if err != nil {
		return fmt.Errorf("could not connect signaling server : %s (reason : %s)", url, err)
	}

	return nil
}

func (sc *signalingClient) close() {
	if sc.socket != nil {
		sc.socket.Close()
	}
}

func (sc *signalingClient) getOffer() (*signalMessage, error) {
	if sc.socket == nil {
		return nil, fmt.Errorf("not connected")
	}

	err := sc.socket.WriteMessage(websocket.TextMessage, signalMessage{Command: "request_offer"}.marshal())
	if err != nil {
		return nil, fmt.Errorf("could not request offer : %s (reason : %s)", sc.url, err)
	}

	_, offerMsg, err := sc.socket.ReadMessage()
	if err != nil {
		return nil, fmt.Errorf("could not recevied offer message : %s (reason : %s)", sc.url, err)
	}

	var offer signalMessage
	err = offer.unmarshal(offerMsg)
	if err != nil {
		return nil, fmt.Errorf("could not unmarshal offer message : %s (reason : %s)", sc.url, err)
	}

	if offer.Sdp.Type == 0 {
		return nil, fmt.Errorf("invalid offer message received : %s (%s)", sc.url, string(offerMsg))
	}

	return &offer, nil
}

func (sc *signalingClient) sendAnswer(answer signalMessage) error {
	byteAnswer, _ := json.Marshal(answer)
	return sc.socket.WriteMessage(websocket.TextMessage, byteAnswer)
}

type ICEServer struct {
	URLs       []string `json:"urls"`
	Username   string   `json:"user_name,omitempty"`
	Credential string   `json:"credential,omitempty"`
}

type signalMessage struct {
	Command    string                    `json:"command"`
	Id         int64                     `json:"id,omitempty"`
	PeerId     int64                     `json:"peer_id,omitempty"`
	Sdp        webrtc.SessionDescription `json:"sdp,omitempty"`
	Candidates []webrtc.ICECandidateInit `json:"candidates"`
	ICEServers []ICEServer               `json:"ice_servers,omitempty"`
}

func (msg signalMessage) marshal() []byte {
	jsonMsg, _ := json.Marshal(msg)
	return jsonMsg
}

func (jsonMsg *signalMessage) unmarshal(msg []byte) error {
	return json.Unmarshal(msg, jsonMsg)
}

func (c *omeClient) report() {

	c.mutex.Lock()
	// Copy and Unlock
	stat := c.stat
	c.mutex.Unlock()

	videoDelay := float64(0)
	audioDelay := float64(0)

	if stat.videoRtpTimestampElapsedMSec != 0 {
		videoDelay = math.Abs(float64(time.Since(stat.startTime).Milliseconds() - stat.videoRtpTimestampElapsedMSec))
	}

	if stat.audioRtpTimestampElapsedMSec != 0 {
		audioDelay = math.Abs(float64(time.Since(stat.startTime).Milliseconds() - stat.audioRtpTimestampElapsedMSec))
	}

	avgBps := int64(float32(stat.totalBytes) / float32(time.Since(stat.startTime).Seconds()) * 8)
	avgFps := float32(stat.totalVideoFrames) / float32(time.Since(stat.startTime).Seconds())

	fmt.Printf("%c[32m[%s]%c[0m\n\trunning_time(%d ms) total_packets(%d) packet_loss(%d)\n\tlast_video_delay (%.1f ms) last_audio_delay (%.1f ms)\n\ttotal_bytes(%s) avg_bps(%s) min_bps(%s) max_bps(%s)\n\ttotal_video_frames(%d) avg_fps(%.2f) min_fps(%.2f) max_fps(%.2f)\n\n",
		27,
		c.name, 27, time.Since(stat.startTime).Milliseconds(), stat.totalRtpPackets, stat.packetLoss, videoDelay, audioDelay,
		ByteCountDecimal(stat.totalBytes), ByteCountDecimal(avgBps), ByteCountDecimal(stat.minBPS), ByteCountDecimal(stat.maxBPS),
		stat.totalVideoFrames, avgFps, stat.minFPS, stat.maxFPS)
}

func (c *omeClient) run(url string) error {

	// Initialize
	c.sc = &signalingClient{}
	c.peerConnection = &webrtc.PeerConnection{}

	// Connect to OME signaling server
	err := c.sc.connect(url)
	if err != nil {
		return err
	}

	// Request offer sdp
	offer, err := c.sc.getOffer()
	if err != nil {
		return err
	}

	// Everything below is the pion-WebRTC API! Thanks for using it ❤️.

	// webrtc config
	config := webrtc.Configuration{
		ICEServers: []webrtc.ICEServer{
			{
				URLs: []string{"stun:stun.l.google.com:19302"},
			},
		},
	}

	for _, iceServer := range offer.ICEServers {
		// The ICEServer information provided by OME has a different format. (user_name => username)
		config.ICEServers = append(config.ICEServers, webrtc.ICEServer{URLs: iceServer.URLs, Username: iceServer.Username, Credential: iceServer.Credential})
	}

	if len(offer.ICEServers) > 0 {
		config.ICETransportPolicy = webrtc.ICETransportPolicyRelay
	}

	// Create a MediaEngine object to configure the supported codec
	engine := &webrtc.MediaEngine{}

	if err := engine.RegisterCodec(webrtc.RTPCodecParameters{
		RTPCodecCapability: webrtc.RTPCodecCapability{MimeType: "video/H264", ClockRate: 90000, Channels: 0, SDPFmtpLine: "", RTCPFeedback: nil},
	}, webrtc.RTPCodecTypeVideo); err != nil {
		panic(err)
	}
	if err := engine.RegisterCodec(webrtc.RTPCodecParameters{
		RTPCodecCapability: webrtc.RTPCodecCapability{MimeType: "video/VP8", ClockRate: 90000, Channels: 0, SDPFmtpLine: "", RTCPFeedback: nil},
	}, webrtc.RTPCodecTypeVideo); err != nil {
		panic(err)
	}
	if err := engine.RegisterCodec(webrtc.RTPCodecParameters{
		RTPCodecCapability: webrtc.RTPCodecCapability{MimeType: "audio/opus", ClockRate: 48000, Channels: 0, SDPFmtpLine: "", RTCPFeedback: nil},
	}, webrtc.RTPCodecTypeAudio); err != nil {
		panic(err)
	}

	webrtcAPI := webrtc.NewAPI(webrtc.WithMediaEngine(engine))
	c.peerConnection, err = webrtcAPI.NewPeerConnection(config)
	if err != nil {
		return err
	}

	// RTP start
	c.peerConnection.OnTrack(func(track *webrtc.TrackRemote, receiver *webrtc.RTPReceiver) {

		fmt.Printf("%s track has started, of type %d: %s \n", c.name, track.PayloadType(), track.Codec().RTPCodecCapability.MimeType)
		c.stat.startTime = time.Now()

		switch track.Kind() {
		case webrtc.RTPCodecTypeVideo:
			c.videoAvailable = true
			c.videoMimeType = track.Codec().MimeType
		case webrtc.RTPCodecTypeAudio:
			c.audioAvailable = true
			c.audioMimeType = track.Codec().MimeType
		}

		go func() {

			lastTime := time.Now()
			lastFrames := int64(0)
			lastBytes := int64(0)

			ticker := time.NewTicker(time.Second * 1)
			// fps/bps calculator, One ticker is sufficient.
			c.once.Do(func() {
				for range ticker.C {

					c.mutex.Lock()
					stat := c.stat
					c.mutex.Unlock()

					currVideoFrames := stat.totalVideoFrames
					currTotalBytes := stat.totalBytes

					fps := float32(float32(currVideoFrames-lastFrames) / float32(time.Since(lastTime).Seconds()))
					bps := int64((float32(currTotalBytes-lastBytes) / float32(time.Since(lastTime).Seconds())) * 8)

					c.mutex.Lock()
					if c.stat.maxFPS == 0 || c.stat.maxFPS < fps {
						c.stat.maxFPS = fps
					}

					if c.stat.minFPS == 0 || c.stat.minFPS > fps {
						c.stat.minFPS = fps
					}

					if c.stat.maxBPS == 0 || c.stat.maxBPS < bps {
						c.stat.maxBPS = bps
					}

					if c.stat.minBPS == 0 || c.stat.minBPS > bps {
						c.stat.minBPS = bps
					}
					c.mutex.Unlock()

					lastTime = time.Now()
					lastFrames = currVideoFrames
					lastBytes = currTotalBytes
				}
			})
		}()

		prev_seq := uint16(0)
		startTimestamp := uint32(0)
		last_report_time := time.Now().AddDate(0, 0, -1)

		for {
			// Read RTP packets being sent to Pion
			rtp, _, readErr := track.ReadRTP()
			if readErr != nil {
				if readErr == io.EOF {
					return
				}
				panic(readErr)
			}

			if startTimestamp == 0 {
				startTimestamp = rtp.Timestamp
			}

			// total packet count
			c.mutex.Lock()

			c.stat.totalRtpPackets++
			c.stat.totalBytes += int64(rtp.PayloadOffset) + int64(len(rtp.Payload))

			// packet loss metric
			if prev_seq != 0 && rtp.SequenceNumber != prev_seq+1 {
				c.stat.packetLoss++
			}
			prev_seq = rtp.SequenceNumber

			need_delay_warn := false

			switch track.Kind() {
			case webrtc.RTPCodecTypeVideo:
				if rtp.Marker {
					c.stat.totalVideoFrames++
					c.stat.videoRtpTimestampElapsedMSec = int64((float64(rtp.Timestamp-startTimestamp) / float64(track.Codec().ClockRate) * 1000))

					rtpDelay := math.Abs(float64(time.Since(c.stat.startTime).Milliseconds() - c.stat.videoRtpTimestampElapsedMSec))
					if rtpDelay > jitterThreshold {
						need_delay_warn = true
					}
				}
			case webrtc.RTPCodecTypeAudio:
				c.stat.audioRtpTimestampElapsedMSec = int64((float64(rtp.Timestamp-startTimestamp) / float64(track.Codec().ClockRate) * 1000))

				rtpDelay := math.Abs(float64(time.Since(c.stat.startTime).Milliseconds() - c.stat.audioRtpTimestampElapsedMSec))
				if rtpDelay > jitterThreshold {
					need_delay_warn = true
				}
			}

			c.mutex.Unlock()

			if need_delay_warn && time.Since(last_report_time).Milliseconds() > delayReportPeriod {
				fmt.Printf("%c[31m[warning]%c[0m delay is too high!\n", 27, 27)
				c.report()

				last_report_time = time.Now()
			}
		}
	})

	c.peerConnection.OnICEConnectionStateChange(func(connectionState webrtc.ICEConnectionState) {
		fmt.Printf("%s connection state has changed %s \n", c.name, connectionState.String())
	})

	// Set remote SessionDescription
	err = c.peerConnection.SetRemoteDescription(offer.Sdp)
	if err != nil {
		return err
	}

	// Set remote IceCandidates
	for _, candidate := range offer.Candidates {
		c.peerConnection.AddICECandidate(candidate)
	}

	// Create an answer
	answerSdp, err := c.peerConnection.CreateAnswer(nil)
	if err != nil {
		return err
	}

	gatherComplete := webrtc.GatheringCompletePromise(c.peerConnection)

	err = c.peerConnection.SetLocalDescription(answerSdp)
	if err != nil {
		return err
	}

	<-gatherComplete

	err = c.sc.sendAnswer(signalMessage{"answer", offer.Id, 0, answerSdp, nil, nil})
	if err != nil {
		return err
	}

	return nil
}

func (c *omeClient) stop() error {

	if c.sc != nil {
		c.sc.close()
	}

	if c.peerConnection != nil {
		c.peerConnection.Close()
	}

	fmt.Printf("%s has stopped\n", c.name)

	return nil
}

// utilities
func ByteCountDecimal(b int64) string {
	const unit = 1000
	if b < unit {
		return fmt.Sprintf("%d B", b)
	}
	div, exp := int64(unit), 0
	for n := b / unit; n >= unit; n /= unit {
		div *= unit
		exp++
	}
	return fmt.Sprintf("%.1f %cB", float64(b)/float64(div), "kMGTPE"[exp])
}
