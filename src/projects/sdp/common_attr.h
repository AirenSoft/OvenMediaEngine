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

// Session Level과 Media Level 양쪽에서 모두 사용될 수 있는 Attribute

#include "sdp_base.h"

class CommonAttr
{
protected:
	CommonAttr();
	~CommonAttr();

	bool			SerializeCommonAttr(ov::String &sdp);
	bool			ParsingCommonAttrLine(char type, std::string content);

public:
	// a=fingerprint:sha-256 D7:81:CF:01:46:FB:2D
	void 			SetFingerprint(const ov::String& algorithm, const ov::String& value);
	virtual ov::String		GetFingerprintAlgorithm();
	virtual ov::String		GetFingerprintValue();

	// a=ice-options:trickle
	void 			SetIceOption(const ov::String& option);
	virtual ov::String		GetIceOption();

	// a=ice-pwd:c32d4070c67e9782bea90a9ab46ea838
	void 			SetIceUfrag(const ov::String& ufrag);
	virtual ov::String		GetIceUfrag();

	// a=ice-ufrag:0dfa46c9
	void 			SetIcePwd(const ov::String& pwd);
	virtual ov::String		GetIcePwd();

private:
	// ice
	ov::String		_ice_option;
	ov::String		_ice_ufrag;
	ov::String		_ice_pwd;
	// dtls
	ov::String		_fingerprint_algorithm;
	ov::String		_fingerprint_value;
};
