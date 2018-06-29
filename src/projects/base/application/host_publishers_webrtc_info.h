//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Gil Hoon Choi
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "base/common_types.h"

class HostPublishersWebRtcInfo
{
public:
    HostPublishersWebRtcInfo();
    virtual ~HostPublishersWebRtcInfo();

    const int GetSessionTimeout() const noexcept;
    void SetSessionTimeout(int session_timeout);

    const int GetCandidatePort() const noexcept;
    void SetCandidatePort(int candidate_port);

    const ov::String GetCandidateProtocol() const noexcept;
    void SetCandidateProtocol(ov::String candidate_protocol);

    const int GetSignallingPort() const noexcept;
    void SetSignallingPort(int signalling_port);

    const ov::String GetSignallingProtocol() const noexcept;
    void SetSignallingProtocol(ov::String signalling_protocol);

    ov::String ToString() const;

private:
    int _session_timeout;
    int _candidate_port;
    ov::String _candidate_protocol;
    int _signalling_port;
    ov::String _signalling_protocol;
};
