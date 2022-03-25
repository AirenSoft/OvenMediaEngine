//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2022 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include "../http_request.h"

namespace http
{
	namespace svr
	{
		namespace h1
		{
			//TODO(h2) : 2022-03-25 | Next : Check if default interceptor works well

			class Http1Request : public HttpRequest
			{
			public:
				// Constructor
				Http1Request(const std::shared_ptr<ov::ClientSocket> &client_socket)
					: HttpRequest(client_socket)
				{
				}

				size_t GetContentLength() const noexcept
				{
					return _http_header_parser.GetContentLength();
				}

				/////////////////////////////////////
				// Implementation of HttpRequest
				/////////////////////////////////////

				ssize_t AppendHeaderData(const std::shared_ptr<const ov::Data> &data) override
				{
					if (GetHeaderParingStatus() == StatusCode::OK)
					{
						// Already parsed
						return 0;
					}
					else if (GetHeaderParingStatus() == StatusCode::PartialContent)
					{
						auto consumed_bytes = _http_header_parser.AppendData(data);
						if (GetHeaderParingStatus() == StatusCode::OK)
						{
							PostProcess();
						}
						return consumed_bytes;
					}
					else
					{
						// Error
						return -1;
					}
				}

				StatusCode GetHeaderParingStatus() const override
				{
					return _http_header_parser.GetStatus();
				}

				Method GetMethod() const noexcept
				{
					return _http_header_parser.GetMethod();
				}

				ov::String GetHttpVersion() const noexcept override
				{
					return _http_header_parser.GetHttpVersion();
				}

				double GetHttpVersionAsNumber() const noexcept override
				{
					return _http_header_parser.GetHttpVersionAsNumber();
				}

				// Path of the URI (including query strings & excluding domain and port)
				// Example: /<app>/<stream>/...?a=b&c=d
				const ov::String &GetRequestTarget() const noexcept override
				{
					return _http_header_parser.GetRequestTarget();
				}

				ov::String GetHeader(const ov::String &key) const noexcept override
				{
					return _http_header_parser.GetHeader(key);
				}

				const bool IsHeaderExists(const ov::String &key) const noexcept override
				{
					return _http_header_parser.IsHeaderExists(key);
				}

			private:
				prot::h1::HttpRequestHeaderParser _http_header_parser;
			};
		} // namespace h1
	} // namespace svr
}  // namespace http