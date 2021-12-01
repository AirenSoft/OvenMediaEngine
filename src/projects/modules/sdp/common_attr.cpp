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

#include "sdp_regex_pattern.h"

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

	// control
	if(!_control.IsEmpty())
	{
		sdp.AppendFormat("a=control:%s\r\n", _control.CStr());
	}

	return true;
}

bool CommonAttr::ParsingCommonAttrLine(char type, std::string content)
{
	std::smatch matches;

	// a=fingerprint:sha-256 D7:81:CF:01:46:FB:2D
	if(content.compare(0, OV_COUNTOF("fi") - 1, "fi") == 0)
	{
		/*
		if(std::regex_search(content, matches, std::regex("^fingerprint:(\\S*) (\\S*)")))
		{
			_fingerprint_algorithm = std::string(matches[1]).c_str();
			_fingerprint_value = std::string(matches[2]).c_str();
		}
		*/
		auto match = SDPRegexPattern::GetInstance()->MatchFingerprint(content.c_str());
		if(match.GetGroupCount() != 2 + 1)
		{
			return false;
		}
		
		_fingerprint_algorithm = match.GetGroupAt(1).GetValue();
		_fingerprint_value = match.GetGroupAt(2).GetValue();
	}
	// a=ice-options:trickle
	else if(content.compare(0, OV_COUNTOF("ice-o") - 1, "ice-o") == 0)
	{
		/*
		if(std::regex_search(content, matches, std::regex("^ice-options:(\\S*)")))
		{
			_ice_option = std::string(matches[1]).c_str();
		}
		*/

		auto match = SDPRegexPattern::GetInstance()->MatchIceOption(content.c_str());
		if(match.GetGroupCount() != 1 + 1)
		{
			return false;
		}
		
		_ice_option = match.GetGroupAt(1).GetValue();
	}
	// a=ice-ufrag:0dfa46c9
	else if(content.compare(0, OV_COUNTOF("ice-u") - 1, "ice-u") == 0)
	{
		/*
		if(std::regex_search(content, matches, std::regex("^ice-ufrag:(\\S*)")))
		{
			_ice_ufrag = std::string(matches[1]).c_str();
		}
		*/
		auto match = SDPRegexPattern::GetInstance()->MatchIceUfrag(content.c_str());
		if(match.GetGroupCount() != 1 + 1)
		{
			return false;
		}
		
		_ice_ufrag = match.GetGroupAt(1).GetValue();
	}
	else if(content.compare(0, OV_COUNTOF("ice-p") - 1, "ice-p") == 0)
	{
		/*
		if(std::regex_search(content, matches, std::regex("^ice-pwd:(\\S*)")))
		{
			_ice_pwd = std::string(matches[1]).c_str();
		}
		*/
		auto match = SDPRegexPattern::GetInstance()->MatchIcePwd(content.c_str());
		if(match.GetGroupCount() != 1 + 1)
		{
			return false;
		}
		
		_ice_pwd = match.GetGroupAt(1).GetValue();
	}
	else if(content.compare(0, OV_COUNTOF("con") - 1, "con") == 0)
	{
		/*
		if(std::regex_search(content, matches, std::regex("^control:(\\S*)")))
		{
			_control = std::string(matches[1]).c_str();
		}
		*/
		auto match = SDPRegexPattern::GetInstance()->MatchControl(content.c_str());
		if(match.GetGroupCount() != 1 + 1)
		{
			return false;
		}
		
		_control = match.GetGroupAt(1).GetValue();
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

ov::String CommonAttr::GetFingerprintAlgorithm() const
{
	return _fingerprint_algorithm;
}

ov::String CommonAttr::GetFingerprintValue() const
{
	return _fingerprint_value;
}

// a=ice-options:trickle
//TODO(getroot): option을 enum값으로 변경
void CommonAttr::SetIceOption(const ov::String &option)
{
	_ice_option = option;
}

ov::String CommonAttr::GetIceOption() const
{
	return _ice_option;
}

// a=ice-pwd:c32d4070c67e9782bea90a9ab46ea838
void CommonAttr::SetIceUfrag(const ov::String &ufrag)
{
	_ice_ufrag = ufrag;
}

ov::String CommonAttr::GetIceUfrag() const
{
	return _ice_ufrag;
}

// a=ice-ufrag:0dfa46c9
void CommonAttr::SetIcePwd(const ov::String &pwd)
{
	_ice_pwd = pwd;
}

ov::String CommonAttr::GetIcePwd() const
{
	return _ice_pwd;
}

void CommonAttr::SetControl(const ov::String &control)
{
	_control = control;
}

ov::String CommonAttr::GetControl() const
{
	return _control;
}
