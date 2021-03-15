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
		if(value.IsEmpty())
		{
			return "";
		}

		ov::String result_string;
		auto val = value.CStr();
		auto length = value.GetLength();
		result_string.SetLength(length);
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
		result_string.SetLength(result_index);
		return result_string;
	}

	std::shared_ptr<Url> Url::Parse(const ov::String &url)
	{
		auto object = std::make_shared<Url>();
		std::smatch matches;
		std::string url_string = url.CStr();

		object->_source = url;

		// <scheme>://<host>[:<port>][/<path/to/resource>][?<query string>]
		// Group 1: <scheme>
		// Group 2: <host>
		// Group 3: :<port>
		// Group 4: <port>
		// Group 5: /<path>
		// Group 6: <path>
		// Group 7: ?<query string>
		// Group 8: <query string>
		if (std::regex_search(url_string, matches, std::regex(R"((.*)://([^:/]+)(:([0-9]+))?(/([^\?]+)?)?(\?([^\?]+)?(.+)?)?)")) == false)
		{
			return nullptr;
		}

		object->_scheme = std::string(matches[1]).c_str();
		object->_host = std::string(matches[2]).c_str();
		object->_port = ov::Converter::ToUInt32(std::string(matches[4]).c_str());
		object->_path = std::string(matches[5]).c_str();
		object->_has_query_string = matches[7].matched;
		if(object->_has_query_string)
		{
			object->_query_string = std::string(matches[8]).c_str();
		}

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

		return std::move(object);
	}

	bool Url::PushBackQueryKey(const ov::String &key)
	{
		if(_has_query_string == true)
		{
			_query_string.Append("&");
		}

		_query_string.Append(key);
		_has_query_string = true;
		_query_parsed = false;

		return true;
	}

	bool Url::PushBackQueryKey(const ov::String &key, const ov::String &value)
	{
		if(_has_query_string == true)
		{
			_query_string.Append("&");
		}

		_query_string.AppendFormat("%s=%s", key.CStr(), Encode(value).CStr());
		_has_query_string = true;
		_query_parsed = false;

		return true;
	}

	// Keep the order of queries.
	bool Url::RemoveQueryKey(const ov::String &remove_key)
	{
		if ((_has_query_string == false))
		{
			return false;
		}

		ov::String new_query_string;
		bool first_query = true;
		// Split the query string into the map
		if (_query_string.IsEmpty() == false)
		{
			const auto &query_list = _query_string.Split("&");
			for (auto &query : query_list)
			{
				auto tokens = query.Split("=", 2);
				ov::String key;
				if (tokens.size() == 2)
				{
					key = tokens[0];	
				}
				else
				{
					key = query;
				}

				if(key.UpperCaseString() != remove_key.UpperCaseString())
				{
					if(first_query == true)
					{
						first_query = false;
					}
					else
					{
						new_query_string.Append("&");
					}
					
					new_query_string.Append(query);
				}
			}
		}

		_query_string = new_query_string;
		_query_parsed = false;

		return true;
	}

	void Url::ParseQueryIfNeeded() const
	{
		if ((_has_query_string == false) || _query_parsed)
		{
			return;
		}

		auto lock_guard = std::lock_guard(_query_map_mutex);

		// DCL
		if (_query_parsed == false)
		{
			_query_parsed = true;

			// Split the query string into the map
			if (_query_string.IsEmpty() == false)
			{
				const auto &query_list = _query_string.Split("&");

				for (auto &query : query_list)
				{
					auto tokens = query.Split("=", 2);

					if (tokens.size() == 2)
					{
						_query_map[tokens[0]] = Decode(tokens[1]);
					}
					else
					{
						_query_map[query] = "";
					}
				}
			}
		}
		else
		{
			// The query is parsed in another thread
		}
	}

	void Url::Print() const
	{
		logi("URL Parser", "%s %s %d %s %s %s %s",
			 _scheme.CStr(), _host.CStr(), _port,
			 _app.CStr(), _stream.CStr(), _file.CStr(), _query_string.CStr());
	}

	ov::String Url::ToUrlString(bool include_query_string) const
	{
		return ov::String::FormatString(
			"%s://%s%s%s%s%s",
			_scheme.CStr(),
			_host.CStr(), (_port > 0) ? ov::String::FormatString(":%d", _port).CStr() : "",
			_path.CStr(), ((include_query_string == false) || _query_string.IsEmpty()) ? "" : "?", include_query_string ? _query_string.CStr() : "");
	}

	ov::String Url::ToString() const
	{
		ov::String description;

		description.AppendFormat(
			"%s://%s%s%s%s%s (app: %s, stream: %s, file: %s)",
			_scheme.CStr(),
			_host.CStr(), (_port > 0) ? ov::String::FormatString(":%d", _port).CStr() : "",
			_path.CStr(), _query_string.IsEmpty() ? "" : "?", _query_string.CStr(),
			_app.CStr(), _stream.CStr(), _file.CStr());

		return description;
	}
}  // namespace ov