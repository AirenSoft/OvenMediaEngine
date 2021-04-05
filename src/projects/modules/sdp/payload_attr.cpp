//==============================================================================
//
//  OvenMediaEngine
//
//  Created by getroot
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#include "payload_attr.h"

PayloadAttr::PayloadAttr()
{
	_id = 0;

	for(bool &flag : _rtcpfb_support_flag)
	{
		flag = false;
	}
}

PayloadAttr::~PayloadAttr()
{
}

void PayloadAttr::SetId(uint8_t id)
{
	_id = id;
}

uint8_t PayloadAttr::GetId() const
{
	return _id;
}

void PayloadAttr::SetRtpmap(const uint8_t payload_type, const ov::String &codec, uint32_t rate, const ov::String &parameters)
{
	_id = payload_type;
	_codec_str = codec;
	_rate = rate;
	_codec_param = parameters;

	if(codec.LowerCaseString() == "h264")
	{
		_codec = SupportCodec::H264;
	}
	else if(codec.LowerCaseString() == "h265")
	{
		_codec = SupportCodec::H265;
	}
	else if(codec.LowerCaseString() == "vp8")
	{
		_codec = SupportCodec::VP8;
	}
	else if(codec.LowerCaseString() == "vp9")
	{
		_codec = SupportCodec::VP9;
	}
	else if(codec.LowerCaseString() == "opus")
	{
		_codec = SupportCodec::OPUS;
	}
	else if(codec.LowerCaseString() == "mpeg4-generic")
	{
		_codec = SupportCodec::MPEG4_GENERIC;
	}
	else if(codec.LowerCaseString() == "red")
	{
		_codec = SupportCodec::RED;
	}
	else if(codec.LowerCaseString() == "rtx")
	{
		_codec = SupportCodec::RTX;
	}
	else
	{
		_codec = SupportCodec::Unknown;
	}
}

// a=rtcp-fb:96 nack pli
void PayloadAttr::EnableRtcpFb(const RtcpFbType &type, const bool on)
{
	_rtcpfb_support_flag[(int)type] = on;
}

PayloadAttr::SupportCodec PayloadAttr::GetCodec() const
{
	return _codec;
}

ov::String PayloadAttr::GetCodecStr() const
{
	return _codec_str;
}

uint32_t PayloadAttr::GetCodecRate() const
{
	return _rate;
}

ov::String PayloadAttr::GetCodecParams() const
{
	return _codec_param;
}

bool PayloadAttr::EnableRtcpFb(const ov::String &type, const bool on)
{
	ov::String type_name = type.UpperCaseString();

	if(type_name == "GOOG_REMB")
	{
		_rtcpfb_support_flag[(int)(RtcpFbType::GoogRemb)] = on;
	}
	else if(type_name == "TRANSPORT_CC")
	{
		_rtcpfb_support_flag[(int)(RtcpFbType::TransportCc)] = on;
	}
	else if(type_name == "CCM_FIR")
	{
		_rtcpfb_support_flag[(int)(RtcpFbType::CcmFir)] = on;
	}
	else if(type_name == "NACK")
	{
		_rtcpfb_support_flag[(int)(RtcpFbType::Nack)] = on;
	}
	else if(type_name == "NACK_PLI")
	{
		_rtcpfb_support_flag[(int)(RtcpFbType::NackPli)] = on;
	}
	else
	{
		return false;
	}

	return true;
}

bool PayloadAttr::IsRtcpFbEnabled(const PayloadAttr::RtcpFbType &type) const
{
	return _rtcpfb_support_flag[(int)type];
}

void PayloadAttr::SetFmtp(const ov::String &fmtp)
{
	// https://tools.ietf.org/html/rfc3640#section-3.3
	// In the case of mpeg4-generic, there are several modes, which are classified by fmtp.
	// fmtp:96 profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config=1190

	if(_codec == SupportCodec::MPEG4_GENERIC)
	{
		auto params = fmtp.Split(";");
		for(const auto &param : params)
		{
			auto key_value = param.Split("=");
			if(key_value.size() != 2)
			{
				// fmtp parsing error
				logd("SDP", "fmtp parsing error : %s", fmtp.CStr());
			}

			auto key = key_value[0];
			auto value = key_value[1];

			if(key.UpperCaseString() == "MODE")
			{
				if(value.UpperCaseString() == "GENERIC")
				{
					_mpeg4_generic_mode = Mpeg4GenericMode::Generic;
				}
				else if(value.UpperCaseString() == "CELP-CBR")
				{
					_mpeg4_generic_mode = Mpeg4GenericMode::CELP_cbr;
				}
				else if(value.UpperCaseString() == "CELP-VBR")
				{
					_mpeg4_generic_mode = Mpeg4GenericMode::CELP_vbr;
				}
				else if(value.UpperCaseString() == "AAC-LBR")
				{
					_mpeg4_generic_mode = Mpeg4GenericMode::AAC_lbr;
				}
				else if(value.UpperCaseString() == "AAC-HBR")
				{
					_mpeg4_generic_mode = Mpeg4GenericMode::AAC_hbr;
				}
				else if(value.UpperCaseString() == "MPS-LBR")
				{
					_mpeg4_generic_mode = Mpeg4GenericMode::AAC_hbr;
				}
				else if(value.UpperCaseString() == "MPS-HBR")
				{
					_mpeg4_generic_mode = Mpeg4GenericMode::AAC_hbr;
				}
			}
		}
	}

	_fmtp = fmtp;
}

ov::String PayloadAttr::GetFmtp() const
{
	return _fmtp;
}
