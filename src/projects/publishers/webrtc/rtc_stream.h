#pragma once

#include <base/ovcrypto/certificate.h>
#include <base/common_types.h>
#include <base/publisher/stream.h>
#include "modules/ice/ice_port.h"
#include "modules/sdp/session_description.h"
#include "modules/rtp_rtcp/rtp_rtcp_defines.h"
#include "rtc_session.h"

#define PAYLOAD_TYPE_OFFSET		100
#define RED_PAYLOAD_TYPE		123
#define	ULPFEC_PAYLOAD_TYPE		124

class RtcStream : public pub::Stream, public RtpRtcpPacketizerInterface
{
public:
	static std::shared_ptr<RtcStream> Create(const std::shared_ptr<pub::Application> application,
	                                         const StreamInfo &info,
	                                         uint32_t worker_count);
	explicit RtcStream(const std::shared_ptr<pub::Application> application,
	                   const StreamInfo &info);
	~RtcStream() final;

	// SDP를 생성하고 관리한다.
	std::shared_ptr<SessionDescription> GetSessionDescription();

	void SendVideoFrame(const std::shared_ptr<MediaPacket> &media_packet) override;
	void SendAudioFrame(const std::shared_ptr<MediaPacket> &media_packet) override;

	// RTP Packetizer를 생성하여 추가한다.
	void AddPacketizer(bool audio, uint32_t id, uint8_t payload_type, uint32_t ssrc);
	std::shared_ptr<RtpPacketizer> GetPacketizer(uint32_t id);

	// RtpRtcpPacketizerInterface Implementation
	// RtpSender, RtcpSender 등에 RtpRtcpSession을 넘겨서 전송 이 함수를 통해 하도록 한다.
	bool OnRtpPacketized(std::shared_ptr<RtpPacket> packet) override;
	bool OnRtcpPacketized(std::shared_ptr<RtcpPacket> packet) override;

private:
	bool Start(uint32_t worker_count) override;
	bool Stop() override;

	// WebRTC의 RTP 에서 사용하는 형태로 변환한다.
	void MakeRtpVideoHeader(const CodecSpecificInfo *info, RTPVideoHeader *rtp_video_header);
	uint16_t AllocateVP8PictureID();

	// VP8 Picture ID
	uint16_t _vp8_picture_id;
	std::shared_ptr<SessionDescription> _offer_sdp;
	std::shared_ptr<Certificate> _certificate;

	// Packetizing을 위해 RtpSender를 이용한다.
	std::map<uint32_t, std::shared_ptr<RtpPacketizer>> _packetizers;
};