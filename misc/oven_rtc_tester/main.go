package main

import (
	"encoding/json"
	"flag"
	"fmt"
	"io"
	"os"
	"os/signal"

	"github.com/gorilla/websocket"
	"github.com/pion/webrtc/v3"
)

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
	for i := 0; i<*numberOfClient; i++ {
		client := new(omeClient)

		client.name = fmt.Sprintf("client_%d", i)
		err := client.run(*url)
		if err != nil {
			fmt.Printf("%s failed to run (reason - %s)\n", client.name, err)
			return
		}
		fmt.Printf("%s has started\n", client.name)
		defer client.stop()

		clients = append(clients, client)
	}

	closed := make(chan os.Signal, 1)
	signal.Notify(closed, os.Interrupt)
	<-closed

	for _, client := range clients {
		if client == nil {
			fmt.Println("Null!")
			continue
		}

		fmt.Printf("%s client finished - total_packet(%d) loss (%d)\n", client.name, client.totalRtpPackets, client.packetLoss)
	}
}

type signalingClient struct {
	url    string
	socket *websocket.Conn
}

type omeClient struct {
	name string

	sc             *signalingClient
	peerConnection *webrtc.PeerConnection

	totalRtpPackets		int64
	packetLoss 			int64
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

	c.peerConnection.OnTrack(func(track *webrtc.TrackRemote, receiver *webrtc.RTPReceiver) {
		fmt.Printf("%s track has started, of type %d: %s \n", c.name, track.PayloadType(), track.Codec().RTPCodecCapability.MimeType)

		prev_seq := uint16(0)
		for {
			// Read RTP packets being sent to Pion
			rtp, _, readErr := track.ReadRTP()
			if readErr != nil {
				if readErr == io.EOF {
					return
				}
				panic(readErr)
			}

			c.totalRtpPackets ++

			if prev_seq != 0 && rtp.SequenceNumber != prev_seq+1 {
				c.packetLoss ++
			}
			prev_seq = rtp.SequenceNumber
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
