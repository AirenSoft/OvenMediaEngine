//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2021 AirenSoft. All rights reserved.
//
//==============================================================================
#include "http_client.h"

#include "../http_private.h"

#define HTTP_CLIENT_READ_BUFFER_SIZE (64 * 1024)

namespace http
{
	namespace clnt
	{
		HttpClient::HttpClient()
			: _socket_pool(ov::SocketPool::GetTcpPool())
		{
			// Default request headers
			_request_header["User-Agent"] = "OvenMediaEngine";
			_request_header["Accept"] = "*/*";
		}

		HttpClient::HttpClient(const std::shared_ptr<ov::SocketPool> &socket_pool)
			: _socket_pool(socket_pool)
		{
			OV_ASSERT2(socket_pool != nullptr);
		}

		HttpClient::~HttpClient()
		{
		}

		void HttpClient::SetBlockingMode(ov::BlockingMode mode)
		{
			_blocking_mode = mode;
		}

		ov::BlockingMode HttpClient::GetBlockingMode() const
		{
			return _blocking_mode;
		}

		void HttpClient::SetConnectionTimeout(int timeout_in_msec)
		{
			_timeout_in_msec = timeout_in_msec;
		}

		int HttpClient::GetConnectionTimeout() const
		{
			return _timeout_in_msec;
		}

		void HttpClient::SetMethod(http::Method method)
		{
			_method = method;
		}

		http::Method HttpClient::GetMethod() const
		{
			return _method;
		}

		void HttpClient::SetRequestHeader(const ov::String &key, const ov::String &value)
		{
			std::lock_guard lock_guard(_request_mutex);

			if (_requested)
			{
				logte("Could not set header after request: %s", key.CStr());
				return;
			}

#if DEBUG
			auto iterator = _request_header.find(key);

			if (iterator != _request_header.end())
			{
				logtw("Old value found: %s for key: %s", iterator->second.CStr(), key.CStr());
			}
#endif	// DEBUG

			_request_header[key] = value;
		}

		ov::String HttpClient::GetRequestHeader(const ov::String &key)
		{
			auto iterator = _request_header.find(key);

			if (iterator == _request_header.end())
			{
				return "";
			}

			return iterator->second;
		}

		const std::unordered_map<ov::String, ov::String, ov::CaseInsensitiveComparator> &HttpClient::GetRequestHeaders() const
		{
			return _request_header;
		}

		std::unordered_map<ov::String, ov::String, ov::CaseInsensitiveComparator> &HttpClient::GetRequestHeaders()
		{
			return _request_header;
		}

		ov::String HttpClient::GetResponseHeader(const ov::String &key)
		{
			return _parser.GetHeader(key);
		}

		const std::unordered_map<ov::String, ov::String, ov::CaseInsensitiveComparator> &HttpClient::GetResponseHeaders() const
		{
			return _parser.GetHeaders();
		}

		std::shared_ptr<const ov::Error> HttpClient::PrepareForRequest(const ov::String &url)
		{
			if (_requested)
			{
				return ov::Error::CreateError("HTTP", "It has already been requested before: %s", _url.CStr());
			}

			if (url.IsEmpty())
			{
				return ov::Error::CreateError("HTTP", "URL must not be empty");
			}

			if (_socket_pool == nullptr)
			{
				return ov::Error::CreateError("HTTP", "Socket pool cannot be nullptr");
			}

			auto parsed_url = ov::Url::Parse(url);

			if (parsed_url == nullptr)
			{
				return ov::Error::CreateError("HTTP", "Could not parse URL: %s", url.CStr());
			}

			bool is_https = false;
			auto scheme = parsed_url->Scheme().UpperCaseString();

			if (scheme == "HTTPS")
			{
				is_https = true;
			}
			else if (scheme == "HTTP")
			{
				is_https = false;
			}
			else
			{
				return ov::Error::CreateError("HTTP", "Unknown scheme: %s, URL: %s", scheme.CStr(), url.CStr());
			}

			_socket = _socket_pool->AllocSocket();

			if (_socket == nullptr)
			{
				return ov::Error::CreateError("HTTP", "Could not create a socket");
			}

			if (((_blocking_mode == ov::BlockingMode::Blocking) ? _socket->MakeBlocking() : _socket->MakeNonBlocking(GetSharedPtr())) == false)
			{
				return ov::Error::CreateError("HTTP", "Could not set blocking mode");
			}

			auto port = parsed_url->Port();
			if (port == 0)
			{
				port = is_https ? 443 : 80;
				parsed_url->SetPort(port);
			}

			_url = url;
			_parsed_url = parsed_url;

			_request_header["Host"] = ov::String::FormatString(
				"%s:%d", _parsed_url->Host().CStr(), _parsed_url->Port());

			return nullptr;
		}

		void HttpClient::SendRequest()
		{
			auto path = _parsed_url->Path();

			if (path.IsEmpty())
			{
				path = "/";
			}

			{
				auto query_string = _parsed_url->Query();
				if (query_string.IsEmpty() == false)
				{
					path.AppendFormat("?%s", query_string.CStr());
				}
			}

			ov::String request_header;

			// Make HTTP 1.1 request header
			logtd("Request resource: %s", path.CStr());

			request_header.AppendFormat("GET %s HTTP/1.1\r\n", path.CStr());

			logtd("Request headers: %zu:", _request_header.size());

			for (auto header : _request_header)
			{
				request_header.AppendFormat("%s: %s\r\n", header.first.CStr(), header.second.CStr());
				logtd("  >> %s: %s", header.first.CStr(), header.second.CStr());
			}

			request_header.Append("\r\n");

			_socket->Send(request_header.ToData(false));
		}

		void HttpClient::RecvResponse()
		{
			auto socket = _socket;
			auto data = std::make_shared<ov::Data>();
			std::shared_ptr<ov::Error> error;

			data->Reserve(HTTP_CLIENT_READ_BUFFER_SIZE);

			while (true)
			{
				error = socket->Recv(data);

				if (error == nullptr)
				{
					if (data->GetLength() == 0)
					{
						// EAGAIN
						if (_blocking_mode == ov::BlockingMode::Blocking)
						{
							continue;
						}

						// Read data next time in Nonblocking mode
						return;
					}

					error = ProcessData(data);
				}

				if (error != nullptr)
				{
					switch (socket->GetState())
					{
						case ov::SocketState::Closed:
							[[fallthrough]];
						case ov::SocketState::Disconnected:
							// Ignore the error
							error = nullptr;
							break;

						default:
							// Free the allocated data
							_response_body = nullptr;
							break;
					}

					break;
				}
			}

			if (_response_handler != nullptr)
			{
				_response_handler(_parser.GetStatusCode(), _response_body, error);
			}

			CleanupVariables();
		}

		std::shared_ptr<ov::Error> HttpClient::ProcessData(const std::shared_ptr<const ov::Data> &data)
		{
			auto remained = data->GetLength();
			auto sub_data = data;

			while (remained > 0)
			{
				switch (_parser.GetParseStatus())
				{
					case StatusCode::OK:
						// Parsing is completed
						_response_body->Append(sub_data);
						remained -= sub_data->GetLength();
						break;

					case StatusCode::PartialContent: {
						auto processed_length = _parser.ProcessData(sub_data);

						switch (_parser.GetParseStatus())
						{
							case StatusCode::OK:
								// Parsing just completed
								// After parsing is completed, this code is executed only once
								PostProcess();
								break;

							case StatusCode::PartialContent:
								// We need more data to parse response
								// In this state, all data must be processed
								OV_ASSERT2((processed_length >= 0LL) && (static_cast<size_t>(processed_length) == data->GetLength()));
								break;

							default:
								OV_ASSERT2(processed_length == -1L);
								return ov::Error::CreateError("HTTP", "Could not parse response");
						}

						sub_data = data->Subdata(processed_length);
						remained -= processed_length;
						break;
					}

					default:
						// If an error occurred when parsing before, this code should not executed under normal circumstances because it was closed after responed.
						OV_ASSERT2(false);
						return ov::Error::CreateError("HTTP", "Invalid parse status: %d", _parser.GetParseStatus());
				}
			}

			return nullptr;
		}

		void HttpClient::PostProcess()
		{
			logtd("Allocating %zu bytes for receiving response data", _parser.GetContentLength());

			_response_body = std::make_shared<ov::Data>(_parser.GetContentLength());
		}

		void HttpClient::Request(const ov::String &url, ResponseHandler response_handler)
		{
			std::lock_guard lock_guard(_request_mutex);

			auto error = PrepareForRequest(url);

			if (error == nullptr)
			{
				OV_ASSERT2(_url.IsEmpty() == false);
				OV_ASSERT2(_parsed_url != nullptr);

				_response_handler = response_handler;

				auto address = ov::SocketAddress(_parsed_url->Host(), _parsed_url->Port());

				logtd("Request an URL: %s (address: %s)...", url.CStr(), address.ToString().CStr());

				error = _socket->Connect(address, _timeout_in_msec);

				if (error != nullptr)
				{
					error = ov::Error::CreateError("HTTP", "Could not connect to %s: %s", url.CStr(), error->ToString().CStr());
				}
			}

			if (error == nullptr)
			{
				_requested = true;

				if (_blocking_mode == ov::BlockingMode::Blocking)
				{
					SendRequest();
					RecvResponse();
				}
			}
			else
			{
				CleanupVariables();

				if (response_handler != nullptr)
				{
					auto http_error = std::dynamic_pointer_cast<const HttpError>(error);
					if (http_error != nullptr)
					{
						response_handler(http_error->GetStatusCode(), _response_body, error);
					}
					else
					{
						response_handler(StatusCode::Unknown, _response_body, error);
					}
				}
			}
		}

		void HttpClient::OnConnected()
		{
			// Response will be processed in OnReadable()
			SendRequest();
		}

		void HttpClient::OnReadable()
		{
			RecvResponse();
		}

		void HttpClient::OnClosed()
		{
			// Ignore
		}

		void HttpClient::CleanupVariables()
		{
			// Clean up variables
			_url.Clear();
			_parsed_url = nullptr;
			_response_handler = nullptr;

			OV_SAFE_RESET(_socket, nullptr, _socket->Close(), _socket);
		}

	}  // namespace clnt
}  // namespace http
