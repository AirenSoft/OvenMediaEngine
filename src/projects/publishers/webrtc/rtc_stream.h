//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
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
#include "rtc_playlist.h"

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

	std::shared_ptr<const SessionDescription> GetSessionDescription(const ov::String &file_name);
	std::shared_ptr<const RtcPlaylist> GetRtcPlaylist(const ov::String &file_name, cmn::MediaCodecId video_codec_id, cmn::MediaCodecId audio_codec_id);

	void SendVideoFrame(const std::shared_ptr<MediaPacket> &media_packet) override;
	void SendAudioFrame(const std::shared_ptr<MediaPacket> &media_packet) override;

	std::shared_ptr<RtxRtpPacket> GetRtxRtpPacket(uint8_t origin_payload_type, uint16_t origin_sequence_number);

	// RtpRtcpPacketizerInterface Implementation
	bool OnRtpPacketized(std::shared_ptr<RtpPacket> packet) override;

private:
	bool Start() override;
	bool Stop() override;
	bool OnStreamUpdated(const std::shared_ptr<info::Stream> &info) override;

	bool IsSupportedCodec(cmn::MediaCodecId codec_id);

	std::shared_ptr<SessionDescription> CreateSessionDescription(const ov::String &file_name = "");

	std::shared_ptr<const RtcMasterPlaylist> GetRtcMasterPlaylist(const ov::String &file_name);
	std::shared_ptr<RtcMasterPlaylist> CreateRtcMasterPlaylist(const ov::String &file_name);

	std::shared_ptr<MediaDescription> MakeVideoDescription() const;
	std::shared_ptr<MediaDescription> MakeAudioDescription() const;

	std::shared_ptr<PayloadAttr> MakePayloadAttr(const std::shared_ptr<const MediaTrack> &track) const;
	std::shared_ptr<PayloadAttr> MakeRtxPayloadAttr(const std::shared_ptr<const MediaTrack> &track) const;

	void MakeRtpVideoHeader(const CodecSpecificInfo *info, RTPVideoHeader *rtp_video_header);
	uint16_t AllocateVP8PictureID();

	bool StorePacketForRTX(std::shared_ptr<RtpPacket> &packet);

	void PushToJitterBuffer(const std::shared_ptr<MediaPacket> &media_packet);
	void PacketizeVideoFrame(const std::shared_ptr<MediaPacket> &media_packet);
	void PacketizeAudioFrame(const std::shared_ptr<MediaPacket> &media_packet);

	void AddPacketizer(const std::shared_ptr<const MediaTrack> &track);
	std::shared_ptr<RtpPacketizer> GetPacketizer(uint32_t track_id);

	void AddRtpHistory(const std::shared_ptr<const MediaTrack> &track);
	std::shared_ptr<RtpHistory> GetHistory(uint8_t origin_payload_type);


	uint32_t GetSsrc(cmn::MediaType media_type);

	// SDP related info
	ov::String _msid;
	ov::String _cname;

	// VP8 Picture ID
	uint16_t _vp8_picture_id;

	std::shared_ptr<Certificate> _certificate;

	// Track ID, Packetizer
	std::shared_mutex _packetizers_lock;
	std::map<uint32_t, std::shared_ptr<RtpPacketizer>> _packetizers;

	// Origin payload type, RtpHistory
	std::map<uint8_t, std::shared_ptr<RtpHistory>> _rtp_history_map;

	uint32_t _video_ssrc = 0;
	uint32_t _video_rtx_ssrc = 0;
	uint32_t _audio_ssrc = 0;

	bool _rtx_enabled = true;
	bool _ulpfec_enabled = true;
	bool _jitter_buffer_enabled = false;
	bool _playout_delay_enabled = false;
	int _playout_delay_min = 0;
	int _playout_delay_max = 0;

	uint32_t _worker_count = 0;

	JitterBufferDelay	_jitter_buffer_delay;

	ov::String _default_playlist_name;

	// Playlist File Name : SessionDescription
	std::map<ov::String, std::shared_ptr<const SessionDescription>> _offer_sdp_map;
	std::shared_mutex _offer_sdp_lock;

	// Playlist File Name : RtcPlaylist
	std::map<ov::String, std::shared_ptr<const RtcMasterPlaylist>> _rtc_master_playlist_map;
	std::shared_mutex _rtc_master_playlist_map_lock;
};