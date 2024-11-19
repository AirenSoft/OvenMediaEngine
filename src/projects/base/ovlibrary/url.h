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

		OV_DEFINE_SETTER_CONST_GETTER(ov::String, bool, SetSource, Source, _source, , , return UpdateUrl(true))
		OV_DEFINE_SETTER_CONST_GETTER(ov::String, bool, SetScheme, Scheme, _scheme, , , return UpdateUrl(false))
		OV_DEFINE_SETTER_CONST_GETTER(ov::String, bool, SetHost, Host, _host, , , return UpdateUrl(false))
		OV_DEFINE_SETTER_CONST_GETTER(uint32_t, bool, SetPort, Port, _port, , , return UpdateUrl(false))
		OV_DEFINE_CONST_GETTER(ov::String, Path, _path, )
		bool SetPath(const ov::String &path);
		OV_DEFINE_SETTER_CONST_GETTER(ov::String, bool, SetApp, App, _app, , , return UpdateUrl(false))
		OV_DEFINE_SETTER_CONST_GETTER(ov::String, bool, SetStream, Stream, _stream, , , return UpdateUrl(false))
		OV_DEFINE_SETTER_CONST_GETTER(ov::String, bool, SetFile, File, _file, , , return UpdateUrl(false))
		OV_DEFINE_SETTER_CONST_GETTER(ov::String, bool, SetId, Id, _id, , , return UpdateUrl(false))
		OV_DEFINE_SETTER_CONST_GETTER(ov::String, bool, SetPassword, Password, _password, , , return UpdateUrl(false))

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

		Url() = default;
		Url(const Url &other);

		std::shared_ptr<Url> Clone() const;

	protected:
		bool ParseFromSource();
		bool UpdateUrl(bool is_source_updated);

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
		ov::String _query_string;
		bool _has_query_string = false;
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