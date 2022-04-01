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
#include "../../hpack/decoder.h"

namespace http
{
	namespace svr
	{
		namespace h2
		{
			class Http2Request : public HttpRequest
			{
			public:
				// Constructor
				Http2Request(const std::shared_ptr<ov::ClientSocket> &client_socket, const std::shared_ptr<hpack::Decoder> &hpack_decoder);

				size_t GetContentLength() const noexcept;

				/////////////////////////////////////
				// Implementation of HttpRequest
				/////////////////////////////////////

				ssize_t AppendHeaderData(const std::shared_ptr<const ov::Data> &data) override;
				StatusCode GetHeaderParingStatus() const override;
				Method GetMethod() const noexcept;
				ov::String GetHttpVersion() const noexcept override;
				double GetHttpVersionAsNumber() const noexcept override;
				// Path of the URI (including query strings & excluding domain and port)
				// Example: /<app>/<stream>/...?a=b&c=d
				const ov::String &GetRequestTarget() const noexcept override;
				ov::String GetHeader(const ov::String &key) const noexcept override;
				const bool IsHeaderExists(const ov::String &key) const noexcept override;

			private:
				std::shared_ptr<hpack::Decoder> _hpack_decoder;
				std::unordered_map<ov::String, ov::String, ov::CaseInsensitiveComparator> _headers;
				StatusCode _parse_status = StatusCode::PartialContent;
			};
		} // namespace h1
	} // namespace svr
}  // namespace http