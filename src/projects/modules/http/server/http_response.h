//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/ovlibrary/converter.h>
#include "../http_datastructure.h"

namespace http
{
	namespace svr
	{
		class HttpExchange;

		class HttpResponse : public ov::EnableSharedFromThis<HttpResponse>
		{
		public:
			HttpResponse(const std::shared_ptr<ov::ClientSocket> &client_socket);
			HttpResponse(const std::shared_ptr<HttpResponse> &http_response);
			~HttpResponse() override = default;

			std::shared_ptr<ov::ClientSocket> GetRemote();
			std::shared_ptr<const ov::ClientSocket> GetRemote() const;

			void SetTlsData(const std::shared_ptr<ov::TlsServerData> &tls_data);
			std::shared_ptr<ov::TlsServerData> GetTlsData();

			StatusCode GetStatusCode() const;
			// Get Reason
			ov::String GetReason();

			// reason = default
			void SetStatusCode(StatusCode status_code);
			// custom reason
			void SetStatusCode(StatusCode status_code, const ov::String &reason);

			// Append a new item to the existing header
			bool AddHeader(const ov::String &key, const ov::String &value);
			// Overwrites the existing value to <value>
			bool SetHeader(const ov::String &key, const ov::String &value);
			const std::vector<ov::String> &GetHeader(const ov::String &key) const;
			bool RemoveHeader(const ov::String &key);

			// Enqueue the data into the queue (This data will be sent when SendResponse() is called)
			// Can be used for response with content-length
			bool AppendData(const std::shared_ptr<const ov::Data> &data);
			bool AppendString(const ov::String &string);
			bool AppendFile(const ov::String &filename);

			uint32_t Response();

			bool Close();

		protected:
			bool IsHeaderSent() const;
			// Get Response Data Size
			size_t GetResponseDataSize() const;
			// Get Response Data List
			const std::vector<std::shared_ptr<const ov::Data>> &GetResponseDataList() const;
			// Get Response Header
			const std::unordered_map<ov::String, std::vector<ov::String>, ov::CaseInsensitiveHash, ov::CaseInsensitiveEqual> &GetResponseHeaderList() const;
			void ResetResponseData();

			// Can be used for response without content-length
			template <typename T>
			bool Send(const T *data)
			{
				return Send(data, sizeof(T));
			}
			virtual bool Send(const void *data, size_t length);
			virtual bool Send(const std::shared_ptr<const ov::Data> &data);
			
		private:
			virtual uint32_t SendHeader();
			virtual uint32_t SendPayload();

			std::shared_ptr<ov::ClientSocket> _client_socket;
			std::shared_ptr<ov::TlsServerData> _tls_data;

			StatusCode _status_code = StatusCode::OK;
			ov::String _reason = StringFromStatusCode(StatusCode::OK);

			bool _is_header_sent = false;
			
			// FIXME(dimiden): It is supposed to be synchronized whenever a packet is sent, but performance needs to be improved
			std::recursive_mutex _response_mutex;

			// https://www.rfc-editor.org/rfc/rfc7230#section-3.2
			// Each header field consists of a case-insensitive field name followed
			// by a colon (":"), optional leading whitespace, the field value, and
			// optional trailing whitespace.

			// https://www.rfc-editor.org/rfc/rfc7540#section-8.1.2
			// Just as in HTTP/1.x, header field names are strings of ASCII
			// characters that are compared in a case-insensitive fashion.  However,
			// header field names MUST be converted to lowercase prior to their
			// encoding in HTTP/2.

			// So _response_header is a map of case insentitive header key and value
			std::unordered_map<ov::String, std::vector<ov::String>, ov::CaseInsensitiveHash, ov::CaseInsensitiveEqual> _response_header;
			std::vector<std::shared_ptr<const ov::Data>> _response_data_list;
			size_t _response_data_size = 0;

			std::vector<ov::String> _default_value{};
		};
	}  // namespace svr
}  // namespace http
