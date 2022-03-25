//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <mutex>

#include "http_request.h"
#include "http_response.h"

namespace http
{
	namespace svr
	{
		class HttpConnection;

		// For HTTP/1.1
		class HttpExchange : public ov::EnableSharedFromThis<HttpExchange>
		{
		public:
			enum class Status : uint8_t
			{
				Init,
				Exchanging, // Exchanging request and response
				Moved, // Control transferred to another thread, header parsed
				Completed, // completed the response
				Upgrade,
				Error
			};

			HttpExchange(const std::shared_ptr<HttpConnection> &connection);
			HttpExchange(const std::shared_ptr<HttpExchange> &exchange); // Copy
			virtual ~HttpExchange() = default;

			// Get connection
			std::shared_ptr<HttpConnection> GetConnection() const;
			
			// Get Status
			Status GetStatus() const;

			// Get Connection Policy
			bool IsKeepAlive() const;

			virtual std::shared_ptr<HttpRequest> GetRequest();
			virtual std::shared_ptr<HttpResponse> GetResponse();

			virtual std::shared_ptr<const HttpRequest> GetRequest() const;
			virtual std::shared_ptr<const HttpResponse> GetResponse() const;

			template <typename T>
			void SetExtra(std::shared_ptr<T> extra)
			{
				_extra = std::move(extra);
			}

			std::any GetExtra() const
			{
				return _extra;
			}

			template <typename T>
			std::shared_ptr<T> GetExtraAs() const
			{
				try
				{
					return std::any_cast<std::shared_ptr<T>>(_extra);
				}
				catch ([[maybe_unused]] const std::bad_any_cast &e)
				{
				}

				return nullptr;
			}

			bool IsWebSocketUpgradeRequest();
			bool IsHttp2UpgradeRequest();

		protected:
			// Check if the request is for upgrade, and 
			bool IsUpgradeRequest();
			bool AcceptUpgrade();
			void SetConnectionPolicyByRequest();
			void SetStatus(Status status);

		private:
			virtual std::shared_ptr<HttpRequest> CreateRequestInstance();
			virtual std::shared_ptr<HttpResponse> CreateResponseInstance();

			std::shared_ptr<HttpConnection> _connection = nullptr;
			std::shared_ptr<HttpRequest> _request = nullptr;
			std::shared_ptr<HttpResponse> _response = nullptr;

			Status _status = Status::Init;

			bool _keep_alive = true; // HTTP/1.1 default

			std::any _extra;
		};
	}  // namespace svr
}  // namespace http
