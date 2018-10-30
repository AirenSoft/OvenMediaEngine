//==============================================================================
//
//  OvenMediaEngine
//
//  Created by getroot
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//
//==============================================================================

#pragma once

#include "sdp_base.h"
#include "common_attr.h"
#include "payload_attr.h"

class SessionDescription;

class MediaDescription : public SdpBase, public CommonAttr
{
public:

	enum class MediaType
	{
		Unknown,
		Video,
		Audio,
		Application
	};

	enum class Direction
	{
		Unknown,
		SendRecv,
		RecvOnly,
		SendOnly,
		Inactive
	};

	enum class SetupType
	{
		Unknown,
		Active,
		Passive,
		ActPass
	};

	explicit MediaDescription(const std::shared_ptr<SessionDescription> &session_description);
	virtual ~MediaDescription();

	bool FromString(const ov::String &desc) override;

	// m=video 9 UDP/TLS/RTP/SAVPF 97
	void SetMediaType(MediaType type);
	bool SetMediaType(const ov::String &type);
	const MediaType GetMediaType();
	void SetPort(uint16_t port);
	uint16_t GetPort();
	void UseDtls(bool flag);
	bool IsUseDtls();

	void AddPayload(const std::shared_ptr<PayloadAttr> &payload);
	const std::shared_ptr<PayloadAttr> GetPayload(uint8_t id);
	const std::shared_ptr<PayloadAttr> GetFirstPayload();

	// a=rtcp-mux
	void UseRtcpMux(bool flag = true);
	bool IsUseRtcpMux();

	// a=sendonly
	void SetDirection(Direction dir);
	bool SetDirection(const ov::String &dir);
	const Direction GetDirection();

	// a=mid:video
	void SetMid(const ov::String &mid);
	const ov::String &GetMid();

	// a=setup:actpass
	void SetSetup(SetupType type);
	bool SetSetup(const ov::String &type);

	// c=IN IP4 0.0.0.0
	void SetConnection(uint8_t version, const ov::String &ip);

	// a=framerate:29.97
	void SetFramerate(float framerate);
	const float GetFramerate();

	// a=rtpmap:96 VP8/50000
	bool AddRtpmap(uint8_t payload_type, const ov::String &codec, uint32_t rate,
	               const ov::String &parameters);

	// a=rtcp-fb:96 nack pli
	bool EnableRtcpFb(uint8_t id, const ov::String &type, bool on);
	void EnableRtcpFb(uint8_t id, const PayloadAttr::RtcpFbType &type, bool on);

	// a=ssrc:2064629418 cname:{b2266c86-259f-4853-8662-ea94cf0835a3}
	void SetCname(uint32_t ssrc, const ov::String &cname);

	uint32_t GetSsrc();
	const ov::String GetCname();

private:
	bool UpdateData(ov::String &sdp) override;
	bool ParsingMediaLine(char type, std::string content);

	MediaType _media_type = MediaType::Unknown;
	ov::String _media_type_str = "UNKNOWN";

	uint16_t _port = 9;
	bool _use_dtls_flag = false;
	ov::String _protocol;

	bool _use_rtcpmux_flag = false;

	Direction _direction = Direction::Unknown;
	ov::String _direction_str = "UNKNOWN";

	ov::String _mid;

	SetupType _setup = SetupType::Unknown;
	ov::String _setup_str = "UNKNOWN";

	uint8_t _connection_ip_version = 4;
	ov::String _connection_ip = "0.0.0.0";

	float _framerate = 0.0f;

	uint32_t _ssrc = 0;
	ov::String _cname;


	std::shared_ptr<SessionDescription> _session_description;
	std::vector<std::shared_ptr<PayloadAttr>> _payload_list;
};
