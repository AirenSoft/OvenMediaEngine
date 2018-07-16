//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Gil Hoon Choi
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "host_base_info.h"
#include "host_publishers_webrtc_info.h"

class HostPublisherInfo : public HostBaseInfo
{
public:
	HostPublisherInfo();
	virtual ~HostPublisherInfo();

	std::shared_ptr<HostPublishersWebRtcInfo> GetWebRtcProperties() const noexcept;
	void SetWebRtcProperties(std::shared_ptr<HostPublishersWebRtcInfo> webrtc_properties);

	ov::String ToString() const;

private:
	std::shared_ptr<HostPublishersWebRtcInfo> _webrtc_properties;
};
