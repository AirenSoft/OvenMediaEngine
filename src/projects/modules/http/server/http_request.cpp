//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "http_request.h"

#include <algorithm>

#include "../http_private.h"

namespace http
{
	namespace svr
	{
		HttpRequest::HttpRequest(const std::shared_ptr<ov::ClientSocket> &client_socket, const std::shared_ptr<RequestInterceptor> &interceptor)
			: _client_socket(client_socket),
			  _interceptor(interceptor)
		{
			OV_ASSERT2(client_socket != nullptr);
		}

		std::shared_ptr<ov::ClientSocket> HttpRequest::GetRemote()
		{
			return _client_socket;
		}

		std::shared_ptr<const ov::ClientSocket> HttpRequest::GetRemote() const
		{
			return _client_socket;
		}

		void HttpRequest::SetTlsData(const std::shared_ptr<ov::TlsData> &tls_data)
		{
			_tls_data = tls_data;
			UpdateUri();
		}

		std::shared_ptr<ov::TlsData> HttpRequest::GetTlsData()
		{
			return _tls_data;
		}

		void HttpRequest::SetConnectionType(RequestConnectionType type)
		{
			_connection_type = type;
			UpdateUri();
		}

		RequestConnectionType HttpRequest::GetConnectionType() const
		{
			return _connection_type;
		}

		ssize_t HttpRequest::ProcessData(const std::shared_ptr<const ov::Data> &data)
		{
			if (_is_header_found)
			{
				// Once the header is found, it should not be called here (since the parse status is already OK)
				OV_ASSERT2(false);
				return 0L;
			}

			const char NewLines[] = "\r\n\r\n";
			// Exclude null character
			const int NewLinesLength = OV_COUNTOF(NewLines) - 1;

			ssize_t used_length = data->GetLength();

			// Since the header has not been parsed yet, try to parse the header every time data comes in
			ssize_t previous_length = _request_string.GetLength();

			// ov::String is binary-safe
			_request_string.Append(data->GetDataAs<char>(), data->GetLength());
			ssize_t newline_position = _request_string.IndexOf(&(NewLines[0]));

			if (newline_position >= 0)
			{
				// If the parser find "\r\n\r\n", start parsing
				_request_string.SetLength(newline_position);

				// Used length =
				//             [Length of string after finding "\r\n\r\n"] -
				//             [Length of string before finding] +
				//             [Length of string "\r\n\r\n"]
				used_length = _request_string.GetLength() - previous_length + NewLinesLength;

				_is_header_found = true;
				_parse_status = ParseMessage();

				if (_parse_status == StatusCode::OK)
				{
					// Calculate some informations such as Content length
					PostProcess();
				}
				else
				{
					// An error occurred during parsing
					used_length = -1L;
					_parse_status = StatusCode::BadRequest;
				}
			}
			else
			{
				// Need more data

				// Check if data consists of non-binary data
				auto data_to_check = data->GetDataAs<char>();
				auto remained = data->GetLength();

				while (remained > 0)
				{
					char character = *data_to_check;

					// isprint(): 32 <= character <= 126
					// isspace(): 9 <= character <= 13
					// reference: https://en.cppreference.com/w/cpp/string/byte/isprint
					if (::isprint(character) || ::isspace(character))
					{
						character++;
						remained--;

						continue;
					}
					else
					{
						// Binary data found
						used_length = -1;
						_parse_status = StatusCode::BadRequest;
						break;
					}
				}
			}

			return used_length;
		}

		StatusCode HttpRequest::ParseMessage()
		{
			// RFC7230 - 3. Message Format
			// HTTP-message   = start-line
			//                  *( header-field CRLF )
			//                  CRLF
			//                  [ message-body ]

			// RFC7230 - 3.1. Start Line
			// start-line     = request-line / status-line

			// Tokenize by "\r\n"
			std::vector<ov::String> tokens = _request_string.Split("\r\n");

			StatusCode status_code;

			status_code = ParseRequestLine(tokens[0]);
			tokens.erase(tokens.begin());

			if (status_code == StatusCode::OK)
			{
				for (const ov::String &token : tokens)
				{
					status_code = ParseHeader(token);

					if (status_code != StatusCode::OK)
					{
						break;
					}
				}
			}

			logtd("Request Headers: %ld:", _request_header.size());

			std::for_each(_request_header.begin(), _request_header.end(), [](const auto &pair) -> void {
				logtd("\t>> %s: %s", pair.first.CStr(), pair.second.CStr());
			});

			return status_code;
		}

#define HTTP_COMPARE_METHOD(text, value_if_matches) \
	if (method == text)                             \
	{                                               \
		_method = value_if_matches;                 \
	}

		StatusCode HttpRequest::ParseRequestLine(const ov::String &line)
		{
			// RFC7230 - 3.1.1. Request Line
			// request-line   = method SP request-target SP HTTP-version CRLF
			ssize_t first_space_index = line.IndexOf(' ');
			ssize_t last_space_index = line.IndexOfRev(' ');

			if ((first_space_index < 0) || (last_space_index < 0) || (first_space_index == last_space_index))
			{
				logtw("Invalid space index: first: %d, last: %d, line: %s", first_space_index, last_space_index, line.CStr());
				return StatusCode::BadRequest;
			}

			_method = Method::Unknown;

			// RFC7231 - 4. Request Methods
			ov::String method = line.Left(static_cast<size_t>(first_space_index));

			HTTP_COMPARE_METHOD("GET", Method::Get);
			HTTP_COMPARE_METHOD("HEAD", Method::Head);
			HTTP_COMPARE_METHOD("POST", Method::Post);
			HTTP_COMPARE_METHOD("PUT", Method::Put);
			HTTP_COMPARE_METHOD("DELETE", Method::Delete);
			HTTP_COMPARE_METHOD("CONNECT", Method::Connect);
			HTTP_COMPARE_METHOD("OPTIONS", Method::Options);
			HTTP_COMPARE_METHOD("TRACE", Method::Trace);

			if (_method == Method::Unknown)
			{
				logtw("Unknown method: %s", method.CStr());
				return StatusCode::MethodNotAllowed;
			}

			// RFC7230 - 5.3. Request Target
			// request-target = origin-form
			//            / absolute-form
			//            / authority-form
			//            / asterisk-form
			_request_target = line.Substring(first_space_index + 1, static_cast<size_t>(last_space_index - first_space_index - 1));

			// RFC7230 - 2.6. Protocol Versioning
			// HTTP-version  = HTTP-name "/" DIGIT "." DIGIT
			// HTTP-name     = %x48.54.54.50 ; "HTTP", case-sensitive
			_http_version = line.Substring(last_space_index + 1);

			logtd("Method: [%s], uri: [%s], version: [%s]", method.CStr(), _request_target.CStr(), _http_version.CStr());
			return StatusCode::OK;
		}

		StatusCode HttpRequest::ParseHeader(const ov::String &line)
		{
			// RFC7230 - 3.2.  Header Fields
			// header-field   = field-name ":" OWS field-value OWS
			//
			// field-name     = token
			// field-value    = *( field-content / obs-fold )
			// field-content  = field-vchar [ 1*( SP / HTAB ) field-vchar ]
			// field-vchar    = VCHAR / obs-text
			//
			// obs-fold       = CRLF 1*( SP / HTAB )
			//                ; obsolete line folding
			//                ; see Section 3.2.4

			// Do not handle OBS-FOLD for the following reasons:
			//
			// RFC7230 - 3.2.4.  Field Parsing
			// Historically, HTTP header field values could be extended over
			// multiple lines by preceding each extra line with at least one space
			// or horizontal tab (obs-fold).  This specification deprecates such
			// line folding except within the message/http media type
			// (Section 8.3.1).  A sender MUST NOT generate a message that includes
			// line folding (i.e., that has any field-value that contains a match to
			// the obs-fold rule) unless the message is intended for packaging
			// within the message/http media type.

			ssize_t colon_index = line.IndexOf(':');

			if (colon_index == -1)
			{
				logtw("Invalid header (could not find colon): %s", line.CStr());
				return StatusCode::BadRequest;
			}

			// Convert all header names to uppercase
			ov::String field_name = line.Left(static_cast<size_t>(colon_index)).UpperCaseString();
			// Eliminate OWS(optional white space) to simplify processing
			ov::String field_value = line.Substring(colon_index + 1).Trim();

			_request_header[field_name] = field_value;

			return StatusCode::OK;
		}

		ov::String HttpRequest::GetHeader(const ov::String &key) const noexcept
		{
			return GetHeader(key, "");
		}

		ov::String HttpRequest::GetHeader(const ov::String &key, ov::String default_value) const noexcept
		{
			auto item = _request_header.find(key.UpperCaseString());

			if (item == _request_header.cend())
			{
				return std::move(default_value);
			}

			return item->second;
		}

		const bool HttpRequest::IsHeaderExists(const ov::String &key) const noexcept
		{
			return _request_header.find(key.UpperCaseString()) != _request_header.cend();
		}

		void HttpRequest::PostProcess()
		{
			switch (_method)
			{
				case Method::Get:
					// GET has no HTTP body
					_content_length = 0L;
					break;

				case Method::Post:
					// TODO(dimiden): Need to parse HTTP body if needed
					_content_length = ov::Converter::ToInt64(GetHeader("CONTENT-LENGTH", "0"));
					break;

				default:
					// Another method
					_content_length = ov::Converter::ToInt64(GetHeader("CONTENT-LENGTH", "0"));
					break;
			}

			UpdateUri();
		}

		void HttpRequest::UpdateUri()
		{
			auto host = GetHeader("Host");
			if (host.IsEmpty())
			{
				host = _client_socket->GetLocalAddress()->GetIpAddress();
			}

			ov::String scheme;
			if (GetConnectionType() == RequestConnectionType::HTTP)
			{
				scheme = "http";
			}
			else if (GetConnectionType() == RequestConnectionType::WebSocket)
			{
				scheme = "ws";
			}

			_request_uri = ov::String::FormatString("%s%s://%s%s", scheme.CStr(), (_tls_data != nullptr) ? "s" : "", host.CStr(), _request_target.CStr());
		}

		ov::String HttpRequest::ToString() const
		{
			return ov::String::FormatString("<HttpRequest: %p, Method: %d, uri: %s>", this, static_cast<int>(_method), GetUri().CStr());
		}
	}  // namespace svr
}  // namespace http
