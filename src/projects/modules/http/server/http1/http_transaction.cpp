//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2022 AirenSoft. All rights reserved.
//
//==============================================================================
#include "http_transaction.h"

#include "../../http_private.h"
#include "../http_connection.h"

namespace http
{
	namespace svr
	{
		namespace h1
		{
			HttpTransaction::HttpTransaction(const std::shared_ptr<HttpConnection> &connection)
				: HttpExchange(connection)
			{

			}

			ssize_t HttpTransaction::OnRequestPacketReceived(const std::shared_ptr<const ov::Data> &data)
			{
				// Header parsing is complete and data is being received.
				if (GetRequest()->GetHeaderParingStatus() == StatusCode::OK)
				{
					auto interceptor = GetConnection()->FindInterceptor(GetSharedPtr());

					// Here, payload data is passed to the interceptor rather than stored in the request. The interceptor may or may not store the payload in the request according to each role.
					auto comsumed_bytes = interceptor->OnDataReceived(GetSharedPtr(), data);
					if (comsumed_bytes <= 0)
					{
						SetStatus(Status::Error);
						return -1;
					}

					_received_data_size += comsumed_bytes;

					// TODO(getroot) : In the case of chunked transfer, there is no Content-length header, and it is necessary to parse the chunk to determine the end when the length is 0. Currently, chunked-transfer type request is not used, so it is not supported.

					if (_received_data_size >= GetRequest()->GetContentLength())
					{
						auto result = interceptor->OnRequestCompleted(GetSharedPtr());
						switch (result)
						{
							case InterceptorResult::Completed:
								SetStatus(Status::Completed);
								break;
							case InterceptorResult::Moved:
								SetStatus(Status::Moved);
								break;
							case InterceptorResult::Error:
							default:
								SetStatus(Status::Error);
								return -1;
						}
					}
					else
					{
						SetStatus(Status::Exchanging);
					}

					return comsumed_bytes;
				}
				// The header has not yet been parsed
				else if (GetRequest()->GetHeaderParingStatus() == StatusCode::PartialContent)
				{
					// Put more data to parse header
					auto comsumed_bytes = GetRequest()->AppendHeaderData(data);
					_received_header_size += comsumed_bytes;

					// Check if header parsing is complete
					if (GetRequest()->GetHeaderParingStatus() == StatusCode::PartialContent)
					{
						// Need more data to parse header
						return comsumed_bytes;
					}
					else if (GetRequest()->GetHeaderParingStatus() == StatusCode::OK)
					{
						// Header parsing is done
						logti("Client(%s) is requested uri: [%s]", GetConnection()->GetSocket()->ToString().CStr(), GetRequest()->GetUri().CStr());

						if (IsUpgradeRequest() == true)
						{
							if (AcceptUpgrade() == false)
							{
								SetStatus(Status::Error);
								return -1;
							}

							SetStatus(Status::Upgrade);
							return comsumed_bytes;
						}

						SetConnectionPolicyByRequest();

						// Find interceptor using received header
						auto interceptor = GetConnection()->FindInterceptor(GetSharedPtr());
						if (interceptor == nullptr)
						{
							SetStatus(Status::Error);
							GetResponse()->SetStatusCode(StatusCode::NotFound);
							GetResponse()->Response();
							return -1;
						}

						// Notify to interceptor
						auto need_to_disconnect = interceptor->OnRequestPrepared(GetSharedPtr());
						if (need_to_disconnect == false)
						{
							SetStatus(Status::Error);
							return -1;
						}

						// Check if the request is completed
						// If Content-length is 0, it means that the request is completed.
						if (GetRequest()->GetContentLength() == 0)
						{
							auto result = interceptor->OnRequestCompleted(GetSharedPtr());
							switch (result)
							{
								case InterceptorResult::Completed:
									SetStatus(Status::Completed);
									break;
								case InterceptorResult::Moved:
									SetStatus(Status::Moved);
									break;
								case InterceptorResult::Error:
								default:
									SetStatus(Status::Error);
									return -1;
							}
						}
						else
						{
							SetStatus(Status::Exchanging);
						}

						return comsumed_bytes;
					}
				}

				// Error
				logte("Invalid parse status: %d", GetRequest()->GetHeaderParingStatus());
				return -1;
			}
		}  // namespace h1
	} // namespace svr
}  // namespace http