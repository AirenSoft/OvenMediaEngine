#pragma once

#include "rtp_rtcp_defines.h"
#include "rtp_packetizer.h"
#include "base/ovlibrary/node.h"
#include "base/info/media_track.h"
#include "rtcp_info/rtcp_sr_generator.h"
#include "rtcp_info/sdes.h"
#include "rtcp_info/receiver_report.h"
#include "rtp_frame_jitter_buffer.h"
#include "rtp_minimal_jitter_buffer.h"
#include "rtp_receive_statistics.h"

#define RECEIVER_REPORT_CYCLE_MS	3000
#define SDES_CYCLE_MS 500

class RtpRtcpInterface : public ov::EnableSharedFromThis<RtpRtcpInterface>
{
public:
	virtual void OnRtpFrameReceived(const std::vector<std::shared_ptr<RtpPacket>> &rtp_packets) = 0;
	virtual void OnRtcpReceived(const std::shared_ptr<RtcpInfo> &rtcp_info) = 0;
};

class RtpRtcp : public ov::Node
{
public:
	RtpRtcp(const std::shared_ptr<RtpRtcpInterface> &observer);
	~RtpRtcp() override;

	bool AddRtpSender(uint8_t payload_type, uint32_t ssrc, uint32_t codec_rate, ov::String cname);
	bool AddRtpReceiver(uint32_t track_id, const std::shared_ptr<MediaTrack> &track);
	bool Stop() override;

	bool SendRtpPacket(const std::shared_ptr<RtpPacket> &packet);
	bool SendPLI(uint32_t media_ssrc);
	bool SendFIR(uint32_t media_ssrc);

	uint8_t GetReceivedPayloadType(uint32_t ssrc);

	// These functions help the next node to not have to parse the packet again.
	// Because next node receives raw data format.
	std::shared_ptr<RtpPacket> GetLastSentRtpPacket();
	std::shared_ptr<RtcpPacket> GetLastSentRtcpPacket();

	// Implement Node Interface
	bool OnDataReceivedFromPrevNode(NodeType from_node, const std::shared_ptr<ov::Data> &data) override;
	bool OnDataReceivedFromNextNode(NodeType from_node, const std::shared_ptr<const ov::Data> &data) override;
	
private:
	bool OnRtpReceived(NodeType from_node, const std::shared_ptr<const ov::Data> &data);
	bool OnRtcpReceived(NodeType from_node, const std::shared_ptr<const ov::Data> &data);

	std::shared_ptr<RtpFrameJitterBuffer> GetJitterBuffer(uint8_t payload_type);

    time_t _first_receiver_report_time = 0; // 0 - not received RR packet
    time_t _last_sender_report_time = 0;
    uint64_t _send_packet_sequence_number = 0;

	std::shared_mutex _state_lock;
	std::shared_ptr<RtpRtcpInterface> _observer;
    std::map<uint32_t, std::shared_ptr<RtcpSRGenerator>> _rtcp_sr_generators;
	std::shared_ptr<Sdes> _sdes = nullptr;
	std::shared_ptr<RtcpPacket> _rtcp_sdes = nullptr;
	ov::StopWatch _rtcp_send_stop_watch;
	uint64_t _rtcp_sent_count = 0;
	
	// Receiver SSRC (For RTCP RR, FIR... etc)
	std::unordered_map<uint32_t, std::shared_ptr<RtpReceiveStatistics>> _receive_statistics;
	// Receiver Report Timer
	ov::StopWatch _receiver_report_timer;

	// Jitter buffer
	// payload type : Jitter buffer
	std::unordered_map<uint8_t, std::shared_ptr<RtpFrameJitterBuffer>> _rtp_frame_jitter_buffers;
	std::unordered_map<uint8_t, std::shared_ptr<RtpMinimalJitterBuffer>> _rtp_minimal_jitter_buffers;

	// payload type : MediaTrack Info
	std::unordered_map<uint8_t, std::shared_ptr<MediaTrack>> _tracks;

	// Latest packet
	std::shared_ptr<RtpPacket>		_last_sent_rtp_packet = nullptr;
	std::shared_ptr<RtcpPacket>		_last_sent_rtcp_packet = nullptr;
};
