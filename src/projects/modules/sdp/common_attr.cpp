//==============================================================================
//
//  OvenMediaEngine
//
//  Created by getroot
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#include <regex>
#include "common_attr.h"

CommonAttr::CommonAttr()
{
}

CommonAttr::~CommonAttr()
{
}

bool CommonAttr::SerializeCommonAttr(ov::String &sdp)
{
	sdp = "";

	// FINGERPRINT
	if(!_fingerprint_algorithm.IsEmpty() && !_fingerprint_value.IsEmpty())
	{
		sdp.AppendFormat("a=fingerprint:%s %s\r\n",
		                 _fingerprint_algorithm.CStr(), _fingerprint_value.CStr());
	}

	// ICE-OPTION
	if(!_ice_option.IsEmpty())
	{
		sdp.AppendFormat("a=ice-options:%s\r\n", _ice_option.CStr());
	}

	// ice-ufrag, ice-password
	if(!_ice_ufrag.IsEmpty() && !_ice_pwd.IsEmpty())
	{
		sdp.AppendFormat(
			"a=ice-ufrag:%s\r\n"
			"a=ice-pwd:%s\r\n",
			_ice_ufrag.CStr(),
			_ice_pwd.CStr()
		);
	}

	return true;
}

bool CommonAttr::ParsingCommonAttrLine(char type, std::string content)
{
	std::smatch matches;

	// a=fingerprint:sha-256 D7:81:CF:01:46:FB:2D
	if(content.compare(0, OV_COUNTOF("fi") - 1, "fi") == 0)
	{
		if(std::regex_search(content, matches, std::regex("^fingerprint:(\\S*) (\\S*)")))
		{
			_fingerprint_algorithm = std::string(matches[1]).c_str();
			_fingerprint_value = std::string(matches[2]).c_str();
		}
	}
		// a=ice-options:trickle
	else if(content.compare(0, OV_COUNTOF("ice-o") - 1, "ice-o") == 0)
	{
		if(std::regex_search(content, matches, std::regex("^ice-options:(\\S*)")))
		{
			_ice_option = std::string(matches[1]).c_str();
		}
	}
		// a=ice-ufrag:0dfa46c9
	else if(content.compare(0, OV_COUNTOF("ice-u") - 1, "ice-u") == 0)
	{
		if(std::regex_search(content, matches, std::regex("^ice-ufrag:(\\S*)")))
		{
			_ice_ufrag = std::string(matches[1]).c_str();
		}
	}
	else if(content.compare(0, OV_COUNTOF("ice-p") - 1, "ice-p") == 0)
	{
		if(std::regex_search(content, matches, std::regex("^ice-pwd:(\\S*)")))
		{
			_ice_pwd = std::string(matches[1]).c_str();
		}
	}
	else if(content.compare(0, OV_COUNTOF("fmtp") - 1, "fmtp") == 0)
	{
		if(std::regex_search(content, matches, std::regex("fmtp:(\\d*) (.*)profile-level-id=(.*)")))
		{
			// a=fmtp:97 level-asymmetry-allowed=1;packetization-mode=0;profile-level-id=42e01f
		}
	}
	else if(content.compare(0, OV_COUNTOF("rtcp:") - 1, "rtcp:") == 0)
	{
		if(std::regex_search(content, matches, std::regex("rtcp:(\\d*) IN (.*)")))
		{
			// a=rtcp:9 IN IP4 0.0.0.0
		}
	}
	else
	{
		return false;
	}
	return true;
}

// a=fingerprint:sha-256 D7:81:CF:01:46:FB:2D
//TODO(getroot): algorithm을 enum값으로 변경
void CommonAttr::SetFingerprint(const ov::String &algorithm, const ov::String &value)
{
	_fingerprint_algorithm = algorithm;
	_fingerprint_value = value;
}

ov::String CommonAttr::GetFingerprintAlgorithm()
{
	return _fingerprint_algorithm;
}

ov::String CommonAttr::GetFingerprintValue()
{
	return _fingerprint_value;
}

// a=ice-options:trickle
//TODO(getroot): option을 enum값으로 변경
void CommonAttr::SetIceOption(const ov::String &option)
{
	_ice_option = option;
}

ov::String CommonAttr::GetIceOption()
{
	return _ice_option;
}

// a=ice-pwd:c32d4070c67e9782bea90a9ab46ea838
void CommonAttr::SetIceUfrag(const ov::String &ufrag)
{
	_ice_ufrag = ufrag;
}

ov::String CommonAttr::GetIceUfrag()
{
	return _ice_ufrag;
}

// a=ice-ufrag:0dfa46c9
void CommonAttr::SetIcePwd(const ov::String &pwd)
{
	_ice_pwd = pwd;
}

ov::String CommonAttr::GetIcePwd()
{
	return _ice_pwd;
}
