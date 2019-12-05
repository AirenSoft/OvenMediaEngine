//
// Created by getroot on 19. 12. 2.
//
#include <regex>
#include "url.h"

namespace ov
{
	const std::shared_ptr<Url> Url::Parse(const std::string &url)
	{
		auto object = std::make_shared<Url>();
		std::smatch matches;

		if(std::regex_search(url, matches, std::regex("(.+)://([^:]+)((:)([0-9]+))?/([^\\?/]+)/([^\\?/]+)(/([^\\?]+)?)?((\\?)([^\\?]+)?(.+)?)?")))
		{
			object->_scheme = std::string(matches[1]).c_str();
			object->_domain = std::string(matches[2]).c_str();

			__try
			{
				object->_port = static_cast<uint32_t>(std::stoul(matches[5]));
			}
			catch(const std::invalid_argument& ia)
			{
				object->_port = 0;
			}
			object->_app = std::string(matches[6]).c_str();
			object->_stream = std::string(matches[7]).c_str();
			object->_file = std::string(matches[9]).c_str();
			object->_query = std::string(matches[12]).c_str();
		}
		else
		{
			return nullptr;
		}

		return object;
	}

	void Url::Print()
	{
		logi("URL Parser", "%s %s %d %s %s %s %s", _scheme.CStr(), _domain.CStr(), _port,
				                                   _app.CStr(), _stream.CStr(), _file.CStr(), _query.CStr());
	}

}