//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Gil Hoon Choi
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "provider_info.h"

ProviderInfo::ProviderInfo()
{
}

ProviderInfo::~ProviderInfo()
{
}

const ProviderType ProviderInfo::GetType() const noexcept
{
	return _type;
}

void ProviderInfo::SetType(ProviderType type)
{
	_type = type;
}

void ProviderInfo::SetTypeFromString(ov::String type_string)
{
	_type = ProviderInfo::ProviderTypeFromString(type_string);
}

std::shared_ptr<TranscodeDecodeInfo> ProviderInfo::GetDecode() const noexcept
{
	return _decode;
}

void ProviderInfo::SetDecode(std::shared_ptr<TranscodeDecodeInfo> decode)
{
	_decode = decode;
}

const ov::String ProviderInfo::GetDecodeRef() const noexcept
{
	return _decode_ref;
}

void ProviderInfo::SetDecodeRef(ov::String decode_ref)
{
	_decode_ref = decode_ref;
}

const char *ProviderInfo::StringFromProviderType(ProviderType provider_type) noexcept
{
	switch(provider_type)
	{
		case ProviderType::Webrtc:
			return "webrtc";
		case ProviderType::Rtmp:
			return "rtmp";
		case ProviderType::MpegTs:
			return "mpegts";
		default:
			return "unknown";
	}
}

const ProviderType ProviderInfo::ProviderTypeFromString(ov::String type_string) noexcept
{
	if(type_string == "webrtc")
	{
		return ProviderType::Webrtc;
	}
	else if(type_string == "rtmp")
	{
		return ProviderType::Rtmp;
	}
	else if(type_string == "mpegts")
	{
		return ProviderType::MpegTs;
	}
	else
	{
		return ProviderType::Unknown;
	}
}

ov::String ProviderInfo::ToString() const
{
	ov::String result = ov::String::FormatString("{\"type\": \"%s\"", ProviderInfo::StringFromProviderType(_type));

	if(_decode != nullptr)
	{
		result.AppendFormat(", \"decode\": [%s]", _decode->ToString().CStr());
	}

	result.Append("}");

	return result;
}