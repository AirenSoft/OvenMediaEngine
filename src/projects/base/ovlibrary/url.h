//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "./ovlibrary.h"
#include "./string.h"

namespace ov
{
	// Url is immutable class
	class Url
	{
	public:
		static ov::String Encode(const ov::String &value);
		static ov::String Decode(const ov::String &value);

		// <scheme>://<host>[:<port>][/<path/to/resource>][?<query string>]
		static std::shared_ptr<Url> Parse(const ov::String &url);

		const ov::String &Source() const
		{
			return _source;
		}

		const ov::String &Scheme() const
		{
			return _scheme;
		}

		const ov::String &Host() const
		{
			return _host;
		}

		void SetPort(uint32_t port)
		{
			_port = port;
		}

		const uint32_t &Port() const
		{
			return _port;
		}

		const ov::String &Path() const
		{
			return _path;
		}

		const ov::String &App() const
		{
			return _app;
		}

		const ov::String &Stream() const
		{
			return _stream;
		}

		const ov::String &File() const
		{
			return _file;
		}

		const ov::String &Id() const
		{
			return _id;
		}

		const ov::String &Password() const
		{
			return _password;
		}

		bool HasQueryString() const
		{
			return _has_query_string;
		}

		const ov::String &Query() const
		{
			ParseQueryIfNeeded();
			return _query_string;
		}

		const bool HasQueryKey(ov::String key) const
		{
			ParseQueryIfNeeded();
			if(_query_map.find(key) == _query_map.end())
			{
				return false;
			}

			return true;
		}

		const ov::String GetQueryValue(ov::String key) const
		{
			if(HasQueryKey(key) == false)
			{
				return "";
			}

			return Decode(_query_map[key]);
		}

		const std::map<ov::String, ov::String> &QueryMap() const
		{
			ParseQueryIfNeeded();
			return _query_map;
		}

		bool PushBackQueryKey(const ov::String &key, const ov::String &value);
		bool PushBackQueryKey(const ov::String &key);
		bool RemoveQueryKey(const ov::String &key);

		void Print() const;
		ov::String ToUrlString(bool include_query_string = true) const;
		ov::String ToString() const;

		Url	&operator=(const Url& other) noexcept
		{
			_source = other._source;
			_scheme = other._scheme;
			_host = other._host;
			_port = other._port;
			_path = other._path;
			_has_query_string = other._has_query_string;
			_query_string = other._query_string;
			_query_parsed = other._query_parsed;
			_query_map = other._query_map;
			_app = other._app;
			_stream = other._stream;
			_file = other._file;
			
			return *this;
		}

		Url()
		{
		}

		Url(const Url &other)
		{
			_source = other._source;
			_scheme = other._scheme;
			_host = other._host;
			_port = other._port;
			_path = other._path;
			_has_query_string = other._has_query_string;
			_query_string = other._query_string;
			_query_parsed = other._query_parsed;
			_query_map = other._query_map;
			_app = other._app;
			_stream = other._stream;
			_file = other._file;
		}

		std::shared_ptr<Url> Clone() const
		{
			return std::make_shared<Url>(*this);
		}

	private:
		void ParseQueryIfNeeded() const;

		// Full URL
		ov::String _source;
		ov::String _scheme;
		ov::String _id;
		ov::String _password;
		ov::String _host;
		uint32_t _port = 0;
		ov::String _path;
		bool _has_query_string = false;
		ov::String _query_string;
		// To reduce the cost of parsing the query map, parsing the query only when Query() or QueryMap() is called
		mutable bool _query_parsed = false;
		mutable std::mutex _query_map_mutex;
		mutable std::map<ov::String, ov::String> _query_map;

		// Valid for URLs of the form: <scheme>://<domain>[:<port>]/<app>/<stream>[<file>][?<query string>]
		ov::String _app;
		ov::String _stream;
		ov::String _file;
	};
}  // namespace ov