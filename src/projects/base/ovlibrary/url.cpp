//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#include "url.h"
#include <regex>

namespace ov
{
	std::shared_ptr<const Url> Url::Parse(const std::string &url, bool make_query_map)
	{
		auto object = std::make_shared<Url>();
		std::smatch matches;

		object->_source = url.c_str();

		// <scheme>://<domain>[:<port>][/<path/to/resource>][?<query string>]
		// Group 1: <scheme>
		// Group 2: <domain>
		// Group 3: :<port>
		// Group 4: <port>
		// Group 5: /<path>
		// Group 6: <path>
		// Group 7: ?<query string>
		// Group 8: <query string>
		if (std::regex_search(url, matches, std::regex(R"((.+)://([^:/]+)(:([0-9]+))?(/([^\?]+)?)?(\?([^\?]+)?(.+)?)?)")) == false)
		{
			return nullptr;
		}

		object->_scheme = std::string(matches[1]).c_str();
		object->_domain = std::string(matches[2]).c_str();
		object->_port = ov::Converter::ToUInt32(std::string(matches[4]).c_str());
		object->_path = std::string(matches[5]).c_str();
		object->_query_string = std::string(matches[8]).c_str();

		// split <path> to /<app>/<stream>/<file> (4 tokens)
		auto tokens = object->_path.Split("/");

		switch (tokens.size())
		{
			default:
			case 4:
				object->_file = tokens[3];
				// It is intended that there is no "break;" statement here
			case 3:
				object->_stream = tokens[2];
				// It is intended that there is no "break;" statement here
			case 2:
				object->_app = tokens[1];
				// It is intended that there is no "break;" statement here
			case 1:
			case 0:
				// Nothing to do
				break;
		}

		// Split the query string into the map
		if (make_query_map && (object->_query_string.IsEmpty() == false))
		{
			auto &query_map = object->_query_map;
			const auto &query_list = object->_query_string.Split("&");

			for (auto &query : query_list)
			{
				auto tokens = query.Split("=", 2);

				if (tokens.size() == 2)
				{
					query_map[tokens[0]] = tokens[1];
				}
				else
				{
					query_map[query] = "";
				}
			}
		}

		return std::move(object);
	}

	void Url::Print() const
	{
		logi("URL Parser", "%s %s %d %s %s %s %s",
			 _scheme.CStr(), _domain.CStr(), _port,
			 _app.CStr(), _stream.CStr(), _file.CStr(), _query_string.CStr());
	}

	ov::String Url::ToString() const
	{
		ov::String description;

		description.AppendFormat(
			"%s://%s%s%s%s%s (app: %s, stream: %s, file: %s)",
			_scheme.CStr(),
			_domain.CStr(), (_port > 0) ? ov::String::FormatString(":%d", _port).CStr() : "",
			_path.CStr(), _query_string.IsEmpty() ? "" : "?", _query_string.CStr(),
			_app.CStr(), _stream.CStr(), _file.CStr());

		return description;
	}
}  // namespace ov