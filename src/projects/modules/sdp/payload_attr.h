//==============================================================================
//
//  OvenMediaEngine
//
//  Created by getroot
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include "sdp_base.h"

class PayloadAttr
{
public:
	PayloadAttr();
	virtual ~PayloadAttr();

	enum class SupportCodec
	{
		Unknown,
		VP8,
		VP9,
		H264,
		H265,
		MPEG4_GENERIC,
		OPUS,
		RED,
		RTX
	};

	enum class Mpeg4GenericMode
	{
		Generic,
		CELP_cbr,
		CELP_vbr,
		AAC_lbr,
		AAC_hbr,
		MPS_lbr,
		MPS_hbr
	};

	enum class RtcpFbType
	{
		GoogRemb,
		TransportCc,
		CcmFir,
		Nack,
		NackPli,

		NumberOfRtcpFbType
	};

	void SetId(uint8_t id);
	uint8_t GetId() const;

	void SetRtpmap(const uint8_t payload_type, const ov::String &codec, uint32_t rate, const ov::String &parameters = "");
	SupportCodec GetCodec() const;
	ov::String GetCodecStr() const;
	uint32_t GetCodecRate() const;
	ov::String GetCodecParams() const;

	// a=rtcp-fb:96 nack pli
	bool EnableRtcpFb(const ov::String &type, bool on);
	void EnableRtcpFb(const RtcpFbType &type, bool on);
	bool IsRtcpFbEnabled(const RtcpFbType &type) const;

	// a=fmtp:111 maxplaybackrate=16000; useinbandfec=1; maxaveragebitrate=20000
	void SetFmtp(const ov::String &fmtp);
	ov::String GetFmtp() const;

private:
	uint8_t _id;
	SupportCodec _codec;
	ov::String _codec_str;
	uint32_t _rate;
	ov::String _codec_param;

	Mpeg4GenericMode _mpeg4_generic_mode = Mpeg4GenericMode::Generic;

	bool _rtcpfb_support_flag[(int)(RtcpFbType::NumberOfRtcpFbType)];

	ov::String _fmtp;
};
