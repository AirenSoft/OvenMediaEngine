//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/ovlibrary/ovlibrary.h>
#include <modules/physical_port/physical_port_observer.h>

#include <memory>

#include "../../http_datastructure.h"

namespace http
{
	namespace svr
	{
		enum class RequestConnectionType
		{
			Unknown,
			HTTP,
			WebSocket
		};

		class HttpRequest;
		class RequestInterceptor
		{
		public:
			virtual ~RequestInterceptor() {}

			virtual RequestConnectionType GetConnectionType() = 0;

			// Returns whether the request is an interceptor capable of processing.
			// If this method returns true, it will only pass to this interceptor when data is received in the future, but not to another interceptor.
			virtual bool IsInterceptorForRequest(const std::shared_ptr<const HttpConnection> &client) = 0;

			/// A callback called to initialize request/response immediately after IsInterceptorForRequest()
			///
			/// @param client An instance that contains informations related to HTTP request/response
			///
			/// @return Whether to disconnect with the client
			virtual InterceptorResult OnHttpPrepare(const std::shared_ptr<HttpConnection> &client) = 0;

			/// A callback called if data received from the client.
			///
			/// @param client An instance that contains informations related to HTTP request/response
			/// @param data Received data from the client
			///
			/// @return Whether to disconnect with the client
			virtual InterceptorResult OnHttpData(const std::shared_ptr<HttpConnection> &client, const std::shared_ptr<const ov::Data> &data) = 0;

			/// A callback called if an error occurs during processing.
			///
			/// @param client An instance that contains informations related to HTTP request/response
			/// @param status_code A status code
			///
			/// @remark If the interceptor is the default interceptor of the Server, it can be called before OnHttpPrepare() is called.
			virtual void OnHttpError(const std::shared_ptr<HttpConnection> &client, StatusCode status_code) = 0;

			/// A callback called when the client is disconnects.
			///
			/// @param client An instance that contains informations related to HTTP request/response
			/// @param reason Indicates why this callback was called
			///
			/// @remark The response can no longer be sent to the client in this state because the connection is already disconnected.
			/// This callback is guaranteed to be called at all times even if an error occurs.
			virtual void OnHttpClosed(const std::shared_ptr<HttpConnection> &client, PhysicalPortDisconnectReason reason) = 0;

		protected:
			static const std::shared_ptr<ov::Data> &GetRequestBody(const std::shared_ptr<HttpRequest> &request);
		};
	}  // namespace svr
}  // namespace http
