//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Gil Hoon Choi
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "host_publishers_webrtc_info.h"

HostPublishersWebRtcInfo::HostPublishersWebRtcInfo()
{
}

HostPublishersWebRtcInfo::~HostPublishersWebRtcInfo()
{
}

const int HostPublishersWebRtcInfo::GetSessionTimeout() const noexcept
{
	return _session_timeout;
}

void HostPublishersWebRtcInfo::SetSessionTimeout(int session_timeout)
{
	_session_timeout = session_timeout;
}

const int HostPublishersWebRtcInfo::GetCandidatePort() const noexcept
{
	return _candidate_port;
}

void HostPublishersWebRtcInfo::SetCandidatePort(int candidate_port)
{
	_candidate_port = candidate_port;
}

const ov::String HostPublishersWebRtcInfo::GetCandidateProtocol() const noexcept
{
	return _candidate_protocol;
}

void HostPublishersWebRtcInfo::SetCandidateProtocol(ov::String candidate_protocol)
{
	_candidate_protocol = candidate_protocol;
}

const int HostPublishersWebRtcInfo::GetSignallingPort() const noexcept
{
	return _signalling_port;
}

void HostPublishersWebRtcInfo::SetSignallingPort(int signalling_port)
{
	_signalling_port = signalling_port;
}

const ov::String HostPublishersWebRtcInfo::GetSignallingProtocol() const noexcept
{
	return _signalling_protocol;
}

void HostPublishersWebRtcInfo::SetSignallingProtocol(ov::String signalling_protocol)
{
	_signalling_protocol = signalling_protocol;
}

ov::String HostPublishersWebRtcInfo::ToString() const
{
	ov::String result = ov::String::FormatString("{\"session_timeout\": \"%d\", \"candidate_port\": \"%d\", \"candidate_protocol\": \"%s\", \"signalling_port\": %d, \"signalling_protocol\": \"%s\"}",
	                                             _session_timeout, _candidate_port, _candidate_protocol.CStr(), _signalling_port, _signalling_protocol.CStr());

	return result;
}