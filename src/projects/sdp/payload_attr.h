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

	enum SupportCodec
	{
		VP8,
		H264,
		OPUS
	};

	enum RtcpFbType
	{
		GOOG_REMB,
		TRANSPORT_CC,
		CCM_FIR,
		NACK,
		NACK_PLI,
		NUMBER_OF_RTCPFB_TYPE
	};

	void 				SetId(uint8_t id);
	uint8_t 			GetId();

	// a=rtpmap:97 VP8/50000
	void 				SetRtpmap(const SupportCodec codec, const uint32_t rate, const ov::String parameters="");
	bool 				SetRtpmap(const ov::String& codec, const uint32_t rate, const ov::String parameters="");
	const SupportCodec 	GetCodec();
	const ov::String	GetCodecStr();
	const uint32_t 		GetCodecRate();
	const ov::String	GetCodecParams();

	// a=rtcp-fb:96 nack pli
	bool 				EnableRtcpFb(const ov::String& type, const bool on);
	void 				EnableRtcpFb(const RtcpFbType& type, const bool on);
	bool				IsRtcpFbEnabled(const RtcpFbType& type);

private:
	uint8_t 			_id;
	SupportCodec		_codec;
	ov::String			_codec_str;
	uint32_t 			_rate;
	ov::String			_codec_param;

	bool				_rtcpfb_support_flag[NUMBER_OF_RTCPFB_TYPE];
};
