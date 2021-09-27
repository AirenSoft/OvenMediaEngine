#pragma once

#include <base/ovcrypto/certificate.h>
#include <base/common_types.h>
#include <base/info/stream.h>
#include <base/publisher/stream.h>
#include <modules/ice/ice_port.h>
#include <modules/sdp/session_description.h>
#include <modules/rtp_rtcp/rtp_rtcp_defines.h>
#include <modules/rtp_rtcp/rtp_history.h>
#include <modules/jitter_buffer/jitter_buffer.h>
#include "rtc_session.h"



class RtcStream : public pub::Stream, public RtpPacketizerInterface
{
public:
	static std::shared_ptr<RtcStream> Create(const std::shared_ptr<pub::Application> application,
	                                         const info::Stream &info,
	                                         uint32_t worker_count);

	explicit RtcStream(const std::shared_ptr<pub::Application> application,
	                   const info::Stream &info,
					   uint32_t worker_count);
	~RtcStream() final;

	std::shared_ptr<SessionDescription> GetSessionDescription();

	void SendVideoFrame(const std::shared_ptr<MediaPacket> &media_packet) override;
	void SendAudioFrame(const std::shared_ptr<MediaPacket> &media_packet) override;

	void AddPacketizer(cmn::MediaCodecId codec_id, uint32_t id, uint8_t payload_type, uint32_t ssrc);
	std::shared_ptr<RtpPacketizer> GetPacketizer(uint32_t id);

	void AddRtpHistory(uint8_t origin_payload_type, uint8_t rtx_payload_type, uint32_t rtx_ssrc);
	std::shared_ptr<RtpHistory> GetHistory(uint8_t origin_payload_type);
	std::shared_ptr<RtxRtpPacket> GetRtxRtpPacket(uint8_t origin_payload_type, uint16_t origin_sequence_number);

	// RtpRtcpPacketizerInterface Implementation
	bool OnRtpPacketized(std::shared_ptr<RtpPacket> packet) override;

private:
	bool Start() override;
	bool Stop() override;

	void MakeRtpVideoHeader(const CodecSpecificInfo *info, RTPVideoHeader *rtp_video_header);
	uint16_t AllocateVP8PictureID();

	bool StorePacketForRTX(std::shared_ptr<RtpPacket> &packet);

	void PushToJitterBuffer(const std::shared_ptr<MediaPacket> &media_packet);
	void PacketizeVideoFrame(const std::shared_ptr<MediaPacket> &media_packet);
	void PacketizeAudioFrame(const std::shared_ptr<MediaPacket> &media_packet);

	// VP8 Picture ID
	uint16_t _vp8_picture_id;
	std::shared_ptr<SessionDescription> _offer_sdp;
	std::shared_ptr<Certificate> _certificate;

	// Track ID, Packetizer
	std::shared_mutex _packetizers_lock;
	std::map<uint32_t, std::shared_ptr<RtpPacketizer>> _packetizers;

	// Origin payload type, RtpHistory
	std::map<uint8_t, std::shared_ptr<RtpHistory>> _rtp_history_map;

	bool _rtx_enabled = true;
	bool _ulpfec_enabled = true;
	bool _jitter_buffer_enabled = false;
	uint32_t _worker_count = 0;

	JitterBufferDelay	_jitter_buffer_delay;
};