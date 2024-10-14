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

		OV_DEFINE_SETTER_CONST_GETTER(ov::String, SetSource, Source, _source, )
		OV_DEFINE_SETTER_CONST_GETTER(ov::String, SetScheme, Scheme, _scheme, )
		OV_DEFINE_SETTER_CONST_GETTER(ov::String, SetHost, Host, _host, )
		OV_DEFINE_SETTER_CONST_GETTER(uint32_t, SetPort, Port, _port, )
		OV_DEFINE_CONST_GETTER(ov::String, Path, _path, )
		void SetPath(const ov::String &path);
		OV_DEFINE_SETTER_CONST_GETTER(ov::String, SetApp, App, _app, )
		OV_DEFINE_SETTER_CONST_GETTER(ov::String, SetStream, Stream, _stream, )
		OV_DEFINE_SETTER_CONST_GETTER(ov::String, SetFile, File, _file, )
		OV_DEFINE_SETTER_CONST_GETTER(ov::String, SetId, Id, _id, )
		OV_DEFINE_SETTER_CONST_GETTER(ov::String, SetPassword, Password, _password, )

		bool HasQueryString() const;
		const ov::String &Query() const;
		const bool HasQueryKey(ov::String key) const;
		const ov::String GetQueryValue(ov::String key) const;
		const std::map<ov::String, ov::String> &QueryMap() const;
		bool PushBackQueryKey(const ov::String &key, const ov::String &value);
		bool PushBackQueryKey(const ov::String &key);
		bool RemoveQueryKey(const ov::String &key);

		void Print() const;
		ov::String ToUrlString(bool include_query_string = true) const;
		ov::String ToString() const;

		Url &operator=(const Url &other) noexcept;

		Url() =default;
		Url(const Url &other);

		std::shared_ptr<Url> Clone() const;

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