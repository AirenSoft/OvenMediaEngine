//==============================================================================
//
//  OvenMediaEngine
//
//  Created by getroot
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include <base/ovlibrary/ovlibrary.h>

#include <srtp2/srtp.h>

class SrtpAdapter
{
public:
	SrtpAdapter();
	virtual ~SrtpAdapter();

	bool	SetKey(srtp_ssrc_type_t type, uint64_t crypto_suite, std::shared_ptr<ov::Data> key);


	bool	ProtectRtp(std::shared_ptr<ov::Data> data);

private:

	srtp_ctx_t_* 	_session;
	int 			_rtp_auth_tag_len;
	int 			_rtcp_auth_tag_len;
};
