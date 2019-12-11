#pragma once

#include "./ovlibrary.h"
#include "./string.h"

namespace ov
{
	class Url
	{
	public:
		Url() = default;
		~Url() = default;

		static const std::shared_ptr<Url> Parse(const std::string &url);

		ov::String& Source(){return _source;}

		ov::String& Scheme(){return _scheme;}
		ov::String& Domain(){return _domain;}
		uint32_t Port(){return _port;}
		ov::String& App(){return _app;}
		ov::String& Stream(){return _stream;}
		ov::String& File(){return _file;}
		ov::String& Query(){return _query;}

		void Print();

	private:
		ov::String 	_scheme;
		ov::String 	_domain;
		uint32_t	_port;
		ov::String 	_app;
		ov::String 	_stream;
		ov::String 	_file;
		ov::String 	_query;

		ov::String	_source;
	};
}