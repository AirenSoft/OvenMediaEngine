/*
 *
 * The REGEX pattern in this source code is refers to libsdptransform.
 * https://github.com/ibc/libsdptransform [MIT LICENSE]
 *
 *
 */

#include "session_description.h"
#include <sstream>
#include <regex>

SessionDescription::SessionDescription()
{
}

SessionDescription::~SessionDescription()
{
}

void SessionDescription::Release()
{
	_media_list.clear();
}

bool SessionDescription::UpdateData(ov::String &sdp)
{
	// Session
	sdp.Format(
		"v=%d\r\n"
		"o=%s %u %d %s IP%d %s\r\n"
		"s=%s\r\n"
		"t=%d %d\r\n",
		_version,
		_user_name.CStr(), _session_id, _session_version, _net_type.CStr(), _ip_version, _address.CStr(),
		_session_name.CStr(),
		_start_time, _stop_time
	);

	// Append all media lines to a=group:BUNDLE (Currently, OME only supports BUNDLE only)
	sdp += "a=group:BUNDLE";

	for(auto &media_description : _media_list)
	{
		sdp.AppendFormat(" %s", media_description->GetMid().CStr());
	}

	sdp += "\r\n";
	sdp += "a=group:LS";

	for(auto &media_description : _media_list)
	{
		sdp.AppendFormat(" %s", media_description->GetMid().CStr());
	}

	sdp += "\r\n";

	// msid-semantic
	if(_msid_semantic.IsEmpty() == false)
	{
		sdp.AppendFormat("a=msid-semantic:%s %s\r\n", _msid_semantic.CStr(), _msid_token.CStr());
	}

	// Common Attributes
	ov::String common_attr_text;

	if(SerializeCommonAttr(common_attr_text) == false)
	{
		return false;
	}

	sdp += common_attr_text;

	// Media
	for(auto &media_description : _media_list)
	{
		sdp += media_description->ToString();
	}

	return true;
}

bool SessionDescription::FromString(const ov::String &sdp)
{
	static const std::regex ValidLineRegex("^([a-z])=(.*)");
	std::stringstream sdpstream(sdp.CStr());
	std::string line;

	ov::String media_desc_sdp;
	bool media_level = false;

	while(std::getline(sdpstream, line, '\n'))
	{
		if(line.size() && line[line.length() - 1] == '\r')
		{
			line.pop_back();
		}

		if(!std::regex_search(line, ValidLineRegex))
		{
			continue;
		}
		char type = line[0];
		std::string content = line.substr(2);

		// media라면 다음 m을 만날때까지 또는 stream이 끝날때까지 모아서 media description에 넘긴다.
		if(type == 'm')
		{
			// 새로운 m을 만나면 기존 m level을 파싱
			if(media_level == true)
			{
				auto media_desc = std::make_shared<MediaDescription>();
				if(media_desc->FromString(media_desc_sdp) == false)
				{
					return false;
				}
				AddMedia(media_desc);
			}

			// 초기화
			media_desc_sdp.Clear();
			media_desc_sdp.AppendFormat("%s\n", line.c_str());
			media_level = true;
		}
			// media level이면 계속 모은다.
		else if(media_level == true)
		{
			media_desc_sdp.AppendFormat("%s\n", line.c_str());

			// 만약 sdp의 끝이면 생성
			if(sdpstream.rdbuf()->in_avail() == 0)
			{
				auto media_desc = std::make_shared<MediaDescription>();
				if(media_desc->FromString(media_desc_sdp) == false)
				{
					return false;
				}
				AddMedia(media_desc);
			}
		}
			// media level이 아니면 파싱하여 저장
		else
		{
			if(ParsingSessionLine(type, content) == false)
			{
				return false;
			}
		}
	}

	Update();

	return true;
}

bool SessionDescription::ParsingSessionLine(char type, std::string content)
{
	bool parsing_error = false;
	std::smatch matches;

	switch(type)
	{
		case 'v':
			// v=0
			if(std::regex_search(content, matches, std::regex("^(\\d*)$")))
			{
				if(matches.size() != 1 + 1)
				{
					parsing_error = true;
					break;
				}
				SetVersion(static_cast<uint8_t>(std::stoi(matches[1])));
			}
			break;
		case 'o':
			// o=OvenMediaEngine 1882243660 2 IN IP4 127.0.0.1
			if(std::regex_search(content, matches, std::regex("^(\\S*) (\\d*) (\\d*) (\\S*) IP(\\d) (\\S*)")))
			{
				if(matches.size() != 6 + 1)
				{
					parsing_error = true;
					break;
				}

				SetOrigin(std::string(matches[1]).c_str(),
				          static_cast<uint32_t>(std::stoul(matches[2])),
				          static_cast<uint32_t>(std::stoul(matches[3])),
				          std::string(matches[4]).c_str(),
				          static_cast<uint8_t>(std::stoul(matches[5])),
				          std::string(matches[6]).c_str());
			}
			break;
		case 's':
			// s=-
			if(std::regex_search(content, matches, std::regex("^(.*)")))
			{
				if(matches.size() != 1 + 1)
				{
					parsing_error = true;
					break;
				}

				SetSessionName(std::string(matches[1]).c_str());
			}
			break;
		case 't':
			// t=0 0
			if(std::regex_search(content, matches, std::regex("^(\\d*) (\\d*)")))
			{
				if(matches.size() != 2 + 1)
				{
					parsing_error = true;
					break;
				}

				SetTiming(static_cast<uint32_t>(std::stoul(matches[1])),
				          static_cast<uint32_t>(std::stoul(matches[2])));
			}

			break;
		case 'a':
			// a=group:BUNDLE video audio ...
			if(content.compare(0, OV_COUNTOF("gr") - 1, "gr") == 0)
			{
				if(std::regex_search(content, matches, std::regex("^group:BUNDLE (.*)")))
				{
					if(matches.size() != 1 + 1)
					{
						parsing_error = true;
						break;
					}

					std::string bundle;
					std::stringstream bundles(matches[1]);
					while(std::getline(bundles, bundle, ' '))
					{
						// 당장은 사용하는 곳이 없다. bundle only 이므로...
						_bundles.emplace_back(bundle.c_str());
					}
				}
			}
			// a=msid-semantic:WMS *
			else if(content.compare(0, OV_COUNTOF("ms") - 1, "ms") == 0)
			{
				if(std::regex_search(content, matches, std::regex(R"(^msid-semantic:\s?(\w*) (\S*))")))
				{
					if(matches.size() != 2 + 1)
					{
						parsing_error = true;
						break;
					}

					SetMsidSemantic(std::string(matches[1]).c_str(), std::string(matches[2]).c_str());
				}
			}
			else if(ParsingCommonAttrLine(type, content))
			{
				// Nothing to do
			}
			else
			{
				logw("SDP", "Unknown Attributes : %c=%s", type, content.c_str());
			}

			break;
		default:
			logw("SDP", "Unknown Attributes : %c=%s", type, content.c_str());
	}

	if(parsing_error)
	{
		loge("SDP", "Sdp parsing error : %c=%s", type, content.c_str());
		return false;
	}

	return true;
}

// v=0
void SessionDescription::SetVersion(uint8_t version)
{
	_version = version;
}

uint8_t SessionDescription::GetVersion() const
{
	return _version;
}

// o=OvenMediaEngine 1882243660 2 IN IP4 127.0.0.1
void SessionDescription::SetOrigin(ov::String user_name, uint32_t session_id, uint32_t session_version,
                                   ov::String net_type, uint8_t ip_version, ov::String address)
{
	_user_name = user_name;
	_session_id = session_id;
	_session_version = session_version;
	_net_type = net_type;
	_ip_version = ip_version;
	_address = address;
}

ov::String SessionDescription::GetUserName() const
{
	return _user_name;
}

uint32_t SessionDescription::GetSessionId() const
{
	return _session_id;
}

uint32_t SessionDescription::GetSessionVersion() const
{
	return _session_version;
}

ov::String SessionDescription::GetNetType() const
{
	return _net_type;
}

uint8_t SessionDescription::GetIpVersion() const
{
	return _ip_version;
}

ov::String SessionDescription::GetAddress() const
{
	return _address;
}

// s=-
void SessionDescription::SetSessionName(ov::String session_name)
{
	_session_name = session_name;
}

ov::String SessionDescription::GetSessionName() const
{
	return _session_name;
}

// t=0 0
void SessionDescription::SetTiming(uint32_t start, uint32_t stop)
{
	_start_time = start;
	_stop_time = stop;
}

uint32_t SessionDescription::GetStartTime() const
{
	return _start_time;
}

uint32_t SessionDescription::GetStopTime() const
{
	return _stop_time;
}

// a=msid-semantic:WMS *
void SessionDescription::SetMsidSemantic(const ov::String &semantic, const ov::String &token)
{
	_msid_semantic = semantic;
	_msid_token = token;
}

ov::String SessionDescription::GetMsidSemantic() const
{
	return _msid_semantic;
}

ov::String SessionDescription::GetMsidToken() const
{
	return _msid_token;
}

// m=video 9 UDP/TLS/RTP/SAVPF 97
void SessionDescription::AddMedia(const std::shared_ptr<const MediaDescription> &media)
{
	if(media != nullptr)
	{
		// key는 향후 검색을 위해 사용될 수 있는 값임
		_media_list.push_back(media);
	}
}

const std::shared_ptr<const MediaDescription> SessionDescription::GetMediaByMid(const ov::String &mid) const
{
	for(auto &media : _media_list)
	{
		if(media->GetMid() == mid)
		{
			return media;
		}
	}

	return nullptr;
}

const std::vector<std::shared_ptr<const MediaDescription>> &SessionDescription::GetMediaList() const
{
	return _media_list;
}


const std::shared_ptr<const MediaDescription> SessionDescription::GetFirstMedia() const
{
	if(_media_list.empty() == false)
	{
		return _media_list.front();
	}

	return nullptr;
}

// 아래 값은 Session Level에 없으면 첫번째 m= line에서 값을 가져와야 한다.
ov::String SessionDescription::GetFingerprintAlgorithm() const
{
	ov::String value = CommonAttr::GetFingerprintAlgorithm();

	if(value.IsEmpty())
	{
		auto media = GetFirstMedia();
		if(media != nullptr)
		{
			value = media->GetFingerprintAlgorithm();
		}
	}

	return value;
}

ov::String SessionDescription::GetFingerprintValue() const
{
	ov::String value = CommonAttr::GetFingerprintValue();

	if(value.IsEmpty())
	{
		auto media = GetFirstMedia();
		if(media != nullptr)
		{
			value = media->GetFingerprintValue();
		}
	}

	return value;
}

ov::String SessionDescription::GetIceOption() const
{
	ov::String value = CommonAttr::GetIceOption();

	if(value.IsEmpty())
	{
		auto media = GetFirstMedia();
		if(media != nullptr)
		{
			value = media->GetIceOption();
		}
	}

	return value;
}

ov::String SessionDescription::GetIceUfrag() const
{
	ov::String value = CommonAttr::GetIceUfrag();

	if(value.IsEmpty())
	{
		auto media = GetFirstMedia();
		if(media != nullptr)
		{
			value = media->GetIceUfrag();
		}
	}

	return value;
}

ov::String SessionDescription::GetIcePwd() const
{
	ov::String value = CommonAttr::GetIcePwd();

	if(value.IsEmpty())
	{
		auto media = GetFirstMedia();
		if(media != nullptr)
		{
			value = media->GetIcePwd();
		}
	}

	return value;
}