//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Gil Hoon Choi
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "host_publisher_info.h"

HostPublisherInfo::HostPublisherInfo()
{
}

HostPublisherInfo::~HostPublisherInfo()
{
}

std::shared_ptr<HostPublishersWebRtcInfo> HostPublisherInfo::GetWebRtcProperties() const noexcept
{
	return _webrtc_properties;
}

void HostPublisherInfo::SetWebRtcProperties(std::shared_ptr<HostPublishersWebRtcInfo> webrtc_properties)
{
	_webrtc_properties = webrtc_properties;
}

ov::String HostPublisherInfo::ToString() const
{
	ov::String result = ov::String::FormatString("{\"ip_address\": \"%s\", \"port\": %d, \"protocol\": \"%s\", \"max_connection\": %d",
	                                             _ip_address.CStr(), _port, _protocol.CStr(), _max_connection);

	if(_webrtc_properties != nullptr)
	{
		result.AppendFormat(", \"webrtc_properties\": %s", _webrtc_properties->ToString().CStr());
	}

	result.Append("}");

	return result;
}