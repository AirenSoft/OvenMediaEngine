//==============================================================================
//
//  OvenMediaEngine
//
//  Created by getroot
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#include "media_description.h"
#include "session_description.h"

#include <regex>

MediaDescription::MediaDescription()
{
	UseDtls(true);
	UseRtcpMux(false);
}

MediaDescription::~MediaDescription()
{
}

bool MediaDescription::UpdateData(ov::String &sdp)
{
	if(_media_type == MediaType::Unknown || _direction == Direction::Unknown)
	{
		loge("SDP", "Required value is not defined - MediaType: %s, Direction: %s, SetupType: %s",
		     _media_type_str.CStr(), _direction_str.CStr(), _setup_str.CStr());

		return false;
	}

	// Make m line
	sdp = ov::String::FormatString("m=%s %d %s", _media_type_str.CStr(), _port, _protocol.CStr());

	// Append Payload id
	for(auto &payload : _payload_list)
	{
		if(payload->GetId() == 0)
		{
			return false;
		}

		sdp.AppendFormat(" %d", payload->GetId());
	}

	sdp.AppendFormat(
		"\r\n"
		"c=IN IP%d %s\r\n"
		"a=%s\r\n"
		"a=mid:%s\r\n"
		"a=setup:%s\r\n",
		_connection_ip_version, _connection_ip.CStr(),
		_direction_str.CStr(),
		_mid.CStr(),
		_setup_str.CStr()
	);

	// Common Attributes
	ov::String common_attr_text;
	if(SerializeCommonAttr(common_attr_text) == false)
	{
		return false;
	}

	sdp.Append(common_attr_text);

	if(_framerate)
	{
		sdp.AppendFormat("a=framerate:%.2f\r\n", _framerate);
	}

	if(_use_rtcpmux_flag)
	{
		sdp.AppendFormat("a=rtcp-mux\r\n");
	}

	if(_msid_appdata.IsEmpty() == false)
	{
		sdp.AppendFormat("a=msid:%s %s\r\n", _msid.CStr(), _msid_appdata.CStr());
	}

	// Payloads
	for(auto &payload : _payload_list)
	{
		uint8_t payload_id = payload->GetId();

		sdp.AppendFormat("a=rtpmap:%d %s/%d", payload_id,
		                 payload->GetCodecStr().CStr(),
		                 payload->GetCodecRate());

		if(payload->GetCodecParams().IsEmpty() == false)
		{
			sdp.AppendFormat("/%s", payload->GetCodecParams().CStr());
		}

		sdp.Append("\r\n");

		// a=fmtp:%d packetization-mode=1
		auto fmtp = payload->GetFmtp();

		if(fmtp.IsEmpty() == false)
		{
			sdp.AppendFormat("a=fmtp:%d ", payload_id);
			sdp.Append(fmtp);
			sdp.Append("\r\n");
		}

		if(payload->IsRtcpFbEnabled(PayloadAttr::RtcpFbType::GoogRemb))
		{
			sdp.AppendFormat("a=rtcp-fb:%d goog_remb\r\n", payload_id);
		}
		if(payload->IsRtcpFbEnabled(PayloadAttr::RtcpFbType::TransportCc))
		{
			sdp.AppendFormat("a=rtcp-fb:%d transport-cc\r\n", payload_id);
		}
		if(payload->IsRtcpFbEnabled(PayloadAttr::RtcpFbType::CcmFir))
		{
			sdp.AppendFormat("a=rtcp-fb:%d ccm fir\r\n", payload_id);
		}
		if(payload->IsRtcpFbEnabled(PayloadAttr::RtcpFbType::Nack))
		{
			sdp.AppendFormat("a=rtcp-fb:%d nack\r\n", payload_id);
		}
		if(payload->IsRtcpFbEnabled(PayloadAttr::RtcpFbType::NackPli))
		{
			sdp.AppendFormat("a=rtcp-fb:%d nack pli\r\n", payload_id);
		}
	}

	// SSRCs
	if(_cname.IsEmpty() == false)
	{
		if(_rtx_ssrc != 0)
		{
			sdp.AppendFormat("a=ssrc-group:FID %u %u\r\n", _ssrc, _rtx_ssrc);
		}
		sdp.AppendFormat("a=ssrc:%u cname:%s\r\n", _ssrc, _cname.CStr());
		sdp.AppendFormat("a=ssrc:%u msid:%s %s\r\n", _ssrc, _msid.CStr(), _msid_appdata.CStr());
		sdp.AppendFormat("a=ssrc:%u mslabel:%s\r\n", _ssrc, _msid.CStr());
		sdp.AppendFormat("a=ssrc:%u label:%s\r\n", _ssrc, _msid_appdata.CStr());

		if(_rtx_ssrc != 0)
		{
			sdp.AppendFormat("a=ssrc:%u cname:%s\r\n", _rtx_ssrc, _cname.CStr());
			sdp.AppendFormat("a=ssrc:%u msid:%s %s\r\n", _rtx_ssrc, _msid.CStr(), _msid_appdata.CStr());
			sdp.AppendFormat("a=ssrc:%u mslabel:%s\r\n", _rtx_ssrc, _msid.CStr());
			sdp.AppendFormat("a=ssrc:%u label:%s\r\n", _rtx_ssrc, _msid_appdata.CStr());
		}
	}

	return true;
}


bool MediaDescription::SetMediaType(const ov::String &type)
{
	if(type.UpperCaseString() == "VIDEO")
	{
		SetMediaType(MediaType::Video);
	}
	else if(type.UpperCaseString() == "AUDIO")
	{
		SetMediaType(MediaType::Audio);
	}
	else if(type.UpperCaseString() == "APPLICATION")
	{
		SetMediaType(MediaType::Application);
	}
	else
	{
		return false;
	}

	return true;
}


bool MediaDescription::FromString(const ov::String &desc)
{
	std::stringstream sdpstream(desc.CStr());
	std::string line;

	while(std::getline(sdpstream, line, '\n'))
	{
		if(line.size() && line[line.length() - 1] == '\r')
		{
			line.pop_back();
		}

		char type = line[0];
		std::string content = line.substr(2);

		if(ParsingMediaLine(type, content) == false)
		{
			logw("SDP", "Could not parse line: %c: %s (%s)", type, content.c_str(), line.c_str());
			return false;
		}
	}

	Update();

	return true;
}

bool MediaDescription::SetDirection(const ov::String &dir)
{
	if(dir.UpperCaseString() == "SENDRECV")
	{
		SetDirection(Direction::SendRecv);
	}
	else if(dir.UpperCaseString() == "RECVONLY")
	{
		SetDirection(Direction::RecvOnly);
	}
	else if(dir.UpperCaseString() == "SENDONLY")
	{
		SetDirection(Direction::SendOnly);
	}
	else if(dir.UpperCaseString() == "INACTIVE")
	{
		SetDirection(Direction::Inactive);
	}
	else
	{
		return false;
	}

	return true;
}

bool MediaDescription::ParsingMediaLine(char type, std::string content)
{
	bool parsing_error = false;
	std::smatch matches;

	switch(type)
	{
		case 'm':
			// m=video 9 UDP/TLS/RTP/SAVPF 97

			if(std::regex_search(content, matches, std::regex("^(\\w*) (\\d*) ([\\w\\/]*)(?: (.*))?")))
			{
				if(matches.size() < 4 + 1)
				{
					parsing_error = true;
					break;
				}

				if(!SetMediaType(std::string(matches[1]).c_str()))
				{
					parsing_error = true;
					break;
				}

				SetPort(std::stoul(matches[2]));

				ov::String protocol = std::string(matches[3]).c_str();
				if(protocol.UpperCaseString() == "UDP/TLS/RTP/SAVPF")
				{
					UseDtls(true);
				}
				else if(protocol.UpperCaseString() == "RTP/AVPF")
				{
					UseDtls(false);
				}
				else
				{
					loge("SDP", "Cannot support %s protocol", protocol.CStr());
					parsing_error = true;
					break;
				}

				// Payload를 모두 생성하여 넣는다.
				// 나중에 Payload에 관련된 정보(rtpmap, fmtp, rtcp-fb)가 나오면 파싱하여 해당 Payload에 값을 설정한다.
				std::string payload_number;
				std::stringstream payload_numbers(matches[4]);
				while(std::getline(payload_numbers, payload_number, ' '))
				{
					auto payload = std::make_shared<PayloadAttr>();
					payload->SetId(static_cast<uint8_t>(std::stoul(payload_number)));
					AddPayload(payload);
				}
			}
			else
			{
				// 필수값 이므로 m이 에러가 나면 실패
				parsing_error = true;
			}
			break;
		case 'c':
			// c=IN IP4 0.0.0.0
			if(std::regex_search(content, matches, std::regex("^IN IP(\\d) (\\S*)")))
			{
				if(matches.size() != 2 + 1)
				{
					parsing_error = true;
					break;
				}

				SetConnection(std::stoul(matches[1]), std::string(matches[2]).c_str());
			}
			else
			{
				// 필수값 이므로 m이 에러가 나면 실패
				parsing_error = true;
			}

			break;

		case 'a':
			// For Performance
			if(content.compare(0, OV_COUNTOF("rtp") - 1,"rtp") == 0)
			{
				// a=rtpmap:96 VP8/50000/?
				if(std::regex_search(content,
				                     matches,
				                     std::regex("rtpmap:(\\d*) ([\\w\\-\\.]*)(?:\\s*\\/(\\d*)(?:\\s*\\/(\\S*))?)?")))
				{
					if(matches.size() < 3 + 1)
					{
						parsing_error = true;
						break;
					}

					//TODO(getroot): matches[4]가 없는 경우에도 메모리는 할당 되는지 확인
					AddRtpmap(static_cast<uint8_t>(std::stoul(matches[1])), std::string(matches[2]).c_str(),
					          static_cast<uint32_t>(std::stoul(matches[3])), std::string(matches[4]).c_str());
				}
			}
				// a=rtcp-mux
			else if(content.compare(0, OV_COUNTOF("rtcp-m") - 1,"rtcp-m") == 0)
			{
				if(std::regex_search(content, matches, std::regex("^(rtcp-mux)")))
				{
					UseRtcpMux(true);
				}
			}
			else if(content.compare(0, OV_COUNTOF("rtcp-f") - 1, "rtcp-f") == 0)
			{
				// a=rtcp-fb:96 nack pli
				// pli는 subtype으로 구분해야 하지만 여기서는 type-subtype 형태로 구분한다.
				if(std::regex_search(content,
				                     matches,
				                     std::regex("rtcp-fb:(\\*|\\d*) (.*)")))
				{
					if(matches.size() != 2 + 1)
					{
						parsing_error = true;
						break;
					}

					EnableRtcpFb(static_cast<uint8_t>(std::stoul(matches[1])), std::string(matches[2]).c_str(), true);
				}
			}
			else if(content.compare(0, OV_COUNTOF("mid") - 1, "mid") == 0)
			{
				// a=mid:video,
				if(std::regex_search(content, matches, std::regex("^mid:([^\\s]*)")))
				{
					if(matches.size() != 1 + 1)
					{
						parsing_error = true;
						break;
					}

					SetMid(std::string(matches[1]).c_str());
				}
			}
			else if(content.compare(0, OV_COUNTOF("msid") - 1, "msid") == 0)
			{
				// a=msid:0nm3jPz5YtRJ1NF26G9IKrUCBlWavuwbeiSf 6jHsvxRPcpiEVZbA5QegGowmCtOlh8kTaXJ4
				if(std::regex_search(content, matches, std::regex("^msid:(.*) (.*)")))
				{
					if(matches.size() != 2 + 1)
					{
						parsing_error = true;
						break;
					}

					SetMsid(std::string(matches[1]).c_str(), std::string(matches[2]).c_str());
				}
			}
			else if(content.compare(0, OV_COUNTOF("set") - 1, "set") == 0)
			{
				// a=setup:actpass
				if(std::regex_search(content, matches, std::regex("^setup:(\\w*)")))
				{
					if(matches.size() != 1 + 1)
					{
						parsing_error = true;
						break;
					}

					SetSetup(std::string(matches[1]).c_str());
				}
			}
			else if(content.compare(0, OV_COUNTOF("ss") - 1, "ss") == 0)
			{
				// a=ssrc:2064629418 cname:{b2266c86-259f-4853-8662-ea94cf0835a3}
				if(std::regex_search(content, matches, std::regex("^ssrc:(\\d*) cname(?::(.*))?")))
				{
					if(matches.size() != 2 + 1)
					{
						parsing_error = true;
						break;
					}
					SetSsrc(stoul(matches[1]));
					SetCname(std::string(matches[2]).c_str());
				}
				else if(std::regex_search(content, matches, std::regex("^ssrc-group:FID ([0-9]*) ([0-9]*)")))
				{
					if(matches.size() != 2 + 1)
					{
						parsing_error = true;
						break;
					}

					SetSsrc(stoul(matches[1]));
					SetRtxSsrc(stoul(matches[2]));
				}
			}
			else if(content.compare(0, OV_COUNTOF("fra") - 1, "fra") == 0)
			{
				// a=framerate:29.97
				if(std::regex_search(content, matches, std::regex("^framerate:(\\d+(?:$|\\.\\d+))")))
				{
					if(matches.size() != 1 + 1)
					{
						parsing_error = true;
						break;
					}

					SetFramerate(std::stof(matches[1]));
				}
			}
				// a=sendonly
			else if(content.compare(0, OV_COUNTOF("se") - 1, "se") == 0 ||
			        content.compare(0, OV_COUNTOF("re") - 1, "re") == 0 ||
			        content.compare(0, OV_COUNTOF("in") - 1, "in") == 0)
			{
				if(std::regex_search(content, matches, std::regex("^(sendrecv|recvonly|sendonly|inactive)")))
				{
					if(matches.size() != 1 + 1)
					{
						parsing_error = true;
						break;
					}

					if(!SetDirection(std::string(matches[1]).c_str()))
					{
						parsing_error = true;
						break;
					}
				}
			}
			else if(ParsingCommonAttrLine(type, content))
			{
				// 아무것도 안하기
			}
			else
			{
				//TODO: Implementing of unknown attributes
            	//a=fmtp:112 minptime=10;useinbandfec=1
				logw("SDP", "Unknown Attributes : %c=%s", type, content.c_str());
			}

			break;
		default:
			logw("SDP", "Unknown Attributes : %c=%s", type, content.c_str());
			break;
	}

	if(parsing_error)
	{
		loge("SDP", "Sdp parsing error : %c=%s", type, content.c_str());
		return false;
	}

	return true;
}

// m=video 9 UDP/TLS/RTP/SAVPF 97
void MediaDescription::SetMediaType(const MediaType type)
{
	_media_type = type;
	switch(_media_type)
	{
		case MediaType::Video:
			_media_type_str = "video";
			break;
		case MediaType::Audio:
			_media_type_str = "audio";
			break;
		case MediaType::Application:
			_media_type_str = "application";
			break;
		default:
			_media_type_str = "";
			break;
	}
}

const MediaDescription::MediaType MediaDescription::GetMediaType() const
{
	return _media_type;
}

void MediaDescription::SetPort(uint16_t port)
{
	_port = port;
}

uint16_t MediaDescription::GetPort() const
{
	return _port;
}

void MediaDescription::UseDtls(bool flag)
{
	_use_dtls_flag = flag;

	if(_use_dtls_flag)
	{
		_protocol = "UDP/TLS/RTP/SAVPF";
	}
	else
	{
		_protocol = "RTP/AVPF";
	}
}

bool MediaDescription::IsUseDtls() const
{
	return _use_dtls_flag;
}

void MediaDescription::AddPayload(const std::shared_ptr<PayloadAttr> &payload)
{
	_payload_list.push_back(payload);
}

std::shared_ptr<const PayloadAttr> MediaDescription::GetPayload(uint8_t id) const
{
	for(auto &payload : _payload_list)
	{
		if(payload->GetId() == id)
		{
			return payload;
		}
	}

	return nullptr;
}

std::shared_ptr<PayloadAttr> MediaDescription::GetPayload(uint8_t id)
{
	for(auto &payload : _payload_list)
	{
		if(payload->GetId() == id)
		{
			return payload;
		}
	}

	return nullptr;
}

std::shared_ptr<const PayloadAttr> MediaDescription::GetFirstPayload() const
{
	if(_payload_list.empty() == false)
	{
		return _payload_list.front();
	}

	return nullptr;
}

// a=rtcp-mux
void MediaDescription::UseRtcpMux(bool flag)
{
	_use_rtcpmux_flag = flag;
}

bool MediaDescription::IsUseRtcpMux() const
{
	return _use_rtcpmux_flag;
}

// a=sendonly
void MediaDescription::SetDirection(const Direction dir)
{
	_direction = dir;
	switch(_direction)
	{
		case Direction::SendRecv:
			_direction_str = "sendrecv";
			break;
		case Direction::RecvOnly:
			_direction_str = "recvonly";
			break;
		case Direction::SendOnly:
			_direction_str = "sendonly";
			break;
		case Direction::Inactive:
			_direction_str = "inactive";
			break;
		default:
			_direction_str = "";
			break;
	}
}

const MediaDescription::Direction MediaDescription::GetDirection() const
{
	return _direction;
}

// a=mid:video
void MediaDescription::SetMid(const ov::String &mid)
{
	_mid = mid;
}

const ov::String &MediaDescription::GetMid() const
{
	return _mid;
}

// a=msid:msid appdata
void MediaDescription::SetMsid(const ov::String &msid, const ov::String &msid_appdata)
{
	_msid = msid;
	_msid_appdata = msid_appdata;
}

const ov::String &MediaDescription::GetMsid()
{
	return _msid;
}

const ov::String &MediaDescription::GetMsidAppdata()
{
	return _msid_appdata;
}

// a=setup:actpass
void MediaDescription::SetSetup(const SetupType type)
{
	_setup = type;
	switch(_setup)
	{
		case SetupType::Active:
			_setup_str = "active";
			break;
		case SetupType::Passive:
			_setup_str = "passive";
			break;
		case SetupType::ActPass:
			_setup_str = "actpass";
			break;
		default:
			_setup_str = "actpass"; // default value
	}
}

bool MediaDescription::SetSetup(const ov::String &type)
{
	if(type.UpperCaseString() == "ACTIVE")
	{
		SetSetup(SetupType::Active);
	}
	else if(type.UpperCaseString() == "PASSIVE")
	{
		SetSetup(SetupType::Passive);
	}
	else if(type.UpperCaseString() == "ACTPASS")
	{
		SetSetup(SetupType::ActPass);
	}
	else
	{
		OV_ASSERT(false, "Invalid setup type: %s", type.CStr());
		return false;
	}

	return true;
}

// c=IN IP4 0.0.0.0
void MediaDescription::SetConnection(uint8_t version, const ov::String &ip)
{
	_connection_ip_version = version;
	_connection_ip = ip;
}

void MediaDescription::SetFramerate(const float framerate)
{
	_framerate = framerate;
}

const float MediaDescription::GetFramerate() const
{
	return _framerate;
}

// a=ssrc:2064629418 cname:{b2266c86-259f-4853-8662-ea94cf0835a3}
void MediaDescription::SetCname(const ov::String &cname)
{
	_cname = cname;
}

void MediaDescription::SetSsrc(uint32_t ssrc)
{
	_ssrc = ssrc;
}

void MediaDescription::SetRtxSsrc(uint32_t rtx_ssrc)
{
	_rtx_ssrc = rtx_ssrc;
}

uint32_t MediaDescription::GetSsrc() const
{
	return _ssrc;
}

uint32_t MediaDescription::GetRtxSsrc() const
{
	return _rtx_ssrc;
}


ov::String MediaDescription::GetCname() const
{
	return _cname;
}

// a=rtpmap:96 VP8/50000
bool MediaDescription::AddRtpmap(uint8_t payload_type, const ov::String &codec,
                                 uint32_t rate, const ov::String &parameters)
{
	auto payload = GetPayload(payload_type);

	if(payload == nullptr)
	{
		payload = std::make_shared<PayloadAttr>();
		payload->SetId(payload_type);
		AddPayload(payload);
	}

	payload->SetRtpmap(payload_type, codec, rate, parameters);

	return true;
}

// a=rtcp-fb:96 nack pli
bool MediaDescription::EnableRtcpFb(uint8_t id, const ov::String &type, bool on)
{
	std::shared_ptr<PayloadAttr> payload = GetPayload(id);
	if(payload == nullptr)
	{
		payload = std::make_shared<PayloadAttr>();
		payload->SetId(id);
		AddPayload(payload);
	}

	return payload->EnableRtcpFb(type, on);
}

void MediaDescription::EnableRtcpFb(uint8_t id, const PayloadAttr::RtcpFbType &type, bool on)
{
	std::shared_ptr<PayloadAttr> payload = GetPayload(id);
	if(payload == nullptr)
	{
		payload = std::make_shared<PayloadAttr>();
		payload->SetId(id);
		AddPayload(payload);
	}

	payload->EnableRtcpFb(type, on);
}