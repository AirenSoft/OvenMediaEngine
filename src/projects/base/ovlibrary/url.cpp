//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#include "url.h"
#include <base/ovlibrary/converter.h>
#include <regex>

namespace ov
{
	ov::String Url::Encode(const ov::String &value)
	{
		static char hex_table[] = "0123456789abcdef";
		ov::String encoded_string;

		encoded_string.SetCapacity(value.GetLength() * 3);
		auto plain_buffer = value.CStr();

		while (true)
		{
			unsigned char buf = *plain_buffer;

			if (buf == '\0')
			{
				break;
			}
			else if (::isalnum(buf) || (buf == '-') || (buf == '_') || (buf == '.') || (buf == '~'))
			{
				// Append the character
				encoded_string.Append(buf);
			}
			else if (buf == ' ')
			{
				// Append '+' character if the buffer is a space
				encoded_string.Append('+');
			}
			else
			{
				// Escape
				encoded_string.Append('%');
				encoded_string.Append(hex_table[buf >> 4]);
				encoded_string.Append(hex_table[buf & 15]);
			}

			plain_buffer++;
		}

		return encoded_string;
	}

	ov::String Url::Decode(const ov::String &value)
	{
		ov::String result_string;
		result_string.SetCapacity(value.GetLength() + 1);

		auto val = value.CStr();
		auto length = value.GetLength();

		auto result = result_string.GetBuffer();
		size_t result_index = 0;
		char place_holder[3];
		place_holder[2] = '\0';

		for (size_t index = 0; index < length;)
		{
			char character = val[index];

			if (character == '%')
			{
				// Change '%??' to ascii character
				if ((length - index) > 2)
				{
					auto val1 = val[index + 1];
					auto val2 = val[index + 2];

					if (::isxdigit(val1) && ::isxdigit(val2))
					{
						place_holder[0] = val1;
						place_holder[1] = val2;
						result[result_index] = static_cast<char>(static_cast<int>(::strtol(place_holder, nullptr, 16)));

						index += 3;
						result_index++;

						continue;
					}
				}
			}
			else if (character == '+')
			{
				// Change '+' to ' '
				character = ' ';
			}

			result[result_index] = val[index];

			index++;
			result_index++;
		}

		return result_string;
	}

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
		if (std::regex_search(url, matches, std::regex(R"((.+?)://([^:/]+)(:([0-9]+))?(/([^\?]+)?)?(\?([^\?]+)?(.+)?)?)")) == false)
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
				[[fallthrough]];
			case 3:
				object->_stream = tokens[2];
				[[fallthrough]];
			case 2:
				object->_app = tokens[1];
				[[fallthrough]];
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
					query_map[tokens[0]] = Decode(tokens[1]);
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

	ov::String Url::ToUrlString(bool include_query_string) const
	{
		return ov::String::FormatString(
			"%s://%s%s%s%s%s",
			_scheme.CStr(),
			_domain.CStr(), (_port > 0) ? ov::String::FormatString(":%d", _port).CStr() : "",
			_path.CStr(), ((include_query_string == false) || _query_string.IsEmpty()) ? "" : "?", include_query_string ? _query_string.CStr() : "");
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