//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Gil Hoon Choi
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "publisher_info.h"

PublisherInfo::PublisherInfo()
{
}

PublisherInfo::~PublisherInfo()
{
}

const PublisherType PublisherInfo::GetType() const noexcept
{
	return _type;
}

void PublisherInfo::SetType(PublisherType type)
{
	_type = type;
}

void PublisherInfo::SetTypeFromString(ov::String type_string)
{
	_type = PublisherInfo::PublisherTypeFromString(type_string);
}

std::vector<std::shared_ptr<TranscodeEncodeInfo>> PublisherInfo::GetEncodes() const noexcept
{
	return _encodes;
}

void PublisherInfo::AddEncode(std::shared_ptr<TranscodeEncodeInfo> encode)
{
	_encodes.push_back(encode);
}

const ov::String PublisherInfo::GetEncodesRef() const noexcept
{
	return _encodes_ref;
}

void PublisherInfo::SetEncodesRef(ov::String encodes_ref)
{
	_encodes_ref = encodes_ref;
}

const char *PublisherInfo::StringFromPublisherType(PublisherType publisher_type) noexcept
{
	switch(publisher_type)
	{
		case PublisherType::Webrtc:
			return "webrtc";
		case PublisherType::Dash:
			return "dash";
		case PublisherType::Hls:
			return "hls";
		case PublisherType::Rtmp:
			return "rtmp";
		default:
			return "unknown";
	}
}

const PublisherType PublisherInfo::PublisherTypeFromString(ov::String type_string) noexcept
{
	if(type_string == "webrtc")
	{
		return PublisherType::Webrtc;
	}
	else if(type_string == "dash")
	{
		return PublisherType::Dash;
	}
	else if(type_string == "hls")
	{
		return PublisherType::Hls;
	}
	else if(type_string == "rtmp")
	{
		return PublisherType::Rtmp;
	}
	else
	{
		return PublisherType::Unknown;
	}
}

ov::String PublisherInfo::ToString() const
{
	ov::String result = ov::String::FormatString("{\"type\": \"%s\"", PublisherInfo::StringFromPublisherType(_type));

	if(_encodes.empty() == false)
	{
		result.Append(", \"encodes\": [");

		for(auto iterator = _encodes.begin(); iterator != _encodes.end(); ++iterator)
		{
			if(iterator != _encodes.begin())
			{
				result.Append(",");
			}

			result.AppendFormat("%s", (*iterator)->ToString().CStr());
		}

		result.Append("]");
	}

	result.Append("}");

	return result;
}