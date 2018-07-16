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
		Vp8,
		H264,
		Opus
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
	uint8_t GetId();

	// a=rtpmap:97 VP8/50000
	void SetRtpmap(const SupportCodec codec, uint32_t rate, const ov::String &parameters = "");
	bool SetRtpmap(const ov::String &codec, uint32_t rate, const ov::String &parameters = "");
	const SupportCodec GetCodec();
	const ov::String GetCodecStr();
	const uint32_t GetCodecRate();
	const ov::String GetCodecParams();

	// a=rtcp-fb:96 nack pli
	bool EnableRtcpFb(const ov::String &type, bool on);
	void EnableRtcpFb(const RtcpFbType &type, bool on);
	bool IsRtcpFbEnabled(const RtcpFbType &type);

private:
	uint8_t _id;
	SupportCodec _codec;
	ov::String _codec_str;
	uint32_t _rate;
	ov::String _codec_param;

	bool _rtcpfb_support_flag[(int)(RtcpFbType::NumberOfRtcpFbType)];
};
