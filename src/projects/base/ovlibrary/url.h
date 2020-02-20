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
	class Url
	{
	public:
		static ov::String Encode(const ov::String &value);
		static ov::String Decode(const ov::String &value);

		// <scheme>://<domain>[:<port>][/<path/to/resource>][?<query string>]
		static std::shared_ptr<const Url> Parse(const std::string &url, bool make_query_map = false);

		const ov::String &Source() const
		{
			return _source;
		}

		const ov::String &Scheme() const
		{
			return _scheme;
		}

		const ov::String &Domain() const
		{
			return _domain;
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

		const ov::String &Query() const
		{
			return _query_string;
		}

		const std::map<ov::String, ov::String> &QueryMap() const
		{
			return _query_map;
		}

		void Print() const;
		ov::String ToUrlString(bool include_query_string = true) const;
		ov::String ToString() const;

	private:
		// Full URL
		ov::String _source;
		ov::String _scheme;
		ov::String _domain;
		uint32_t _port;
		ov::String _path;
		ov::String _query_string;
		std::map<ov::String, ov::String> _query_map;

		// Valid for URLs of the form: <scheme>://<domain>[:<port>]/<app>/<stream>[<file>][?<query string>]
		ov::String _app;
		ov::String _stream;
		ov::String _file;
	};
}  // namespace ov