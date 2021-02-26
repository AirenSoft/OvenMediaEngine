//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "modules/ice/ice_candidate.h"

class RtcIceCandidate : public IceCandidate
{
public:
	RtcIceCandidate(ov::String transport, ov::String ip_address, int port, uint16_t sdp_m_line_index, ov::String sdp_mid);
	RtcIceCandidate(uint16_t sdp_m_line_index, ov::String sdp_mid);
	~RtcIceCandidate() override;

	RtcIceCandidate &operator =(const RtcIceCandidate &candidate) = default;

	uint16_t GetSdpMLineIndex() const noexcept;
	void SetSdpMLineIndex(uint16_t sdp_m_line_index) noexcept;

	ov::String GetSdpMid() const noexcept;
	void SetSdpMid(ov::String sdp_mid) noexcept;

	ov::String ToString() const noexcept override;

protected:
	uint16_t _sdp_m_line_index;
	ov::String _sdp_mid;
};
