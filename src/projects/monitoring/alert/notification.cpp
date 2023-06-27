//=============================================================================
//
//	OvenMediaEngine
//
//	Created by Gilhoon Choi
//	Copyright (c) 2023 AirenSoft. All rights reserved.
//
//==============================================================================
#include "notification.h"

#include <modules/http/client/http_client.h>
#include <modules/json_serdes/converters.h>

namespace mon
{
	namespace alrt
	{
		std::shared_ptr<Notification> Notification::Query(const std::shared_ptr<ov::Url> &notification_server_url, uint32_t timeout_msec, const ov::String secret_key, const ov::String message_body)
		{
			auto notification = std::make_shared<Notification>();

			notification->_notification_server_url = notification_server_url;
			notification->_timeout_msec = timeout_msec;
			notification->_secret_key = secret_key;
			notification->_message_body = message_body;
			notification->_status_code = StatusCode::OK;

			notification->Run();

			return notification;
		}

		Notification::StatusCode Notification::GetStatusCode() const
		{
			return _status_code;
		}

		ov::String Notification::GetErrorReason() const
		{
			return _error_reason;
		}

		uint64_t Notification::GetElapsedTime() const
		{
			return _elapsed_msec;
		}

		void Notification::SetStatus(StatusCode code, ov::String reason)
		{
			_status_code = code;
			_error_reason = reason;
		}

		ov::String Notification::GetMessageBody()
		{
			return _message_body;
		}

		void Notification::ParseResponse(const std::shared_ptr<const ov::Data> &data)
		{
			ov::JsonObject object = ov::Json::Parse(data->ToString());
			if (object.IsNull())
			{
				SetStatus(StatusCode::INVALID_DATA_FORMAT, ov::String::FormatString("Json parsing error : a response in the wrong format was received."));
				return;
			}
		}

		void Notification::Run()
		{
			if (_message_body.IsEmpty())
			{
				// Error
				return;
			}

			// Set X-OME-Signature
			auto md_sha1 = ov::MessageDigest::ComputeHmac(ov::CryptoAlgorithm::Sha1, _secret_key.ToData(false), _message_body.ToData(false));
			if (md_sha1 == nullptr)
			{
				// Error
				SetStatus(StatusCode::INTERNAL_ERROR, ov::String::FormatString("Signature creation failed.(Method : HMAC(SHA1), Body length : %d", _message_body.GetLength()));
				return;
			}

			auto signature_sha1_base64 = ov::Base64::Encode(md_sha1, true);

			auto client = std::make_shared<http::clnt::HttpClient>();
			client->SetMethod(http::Method::Post);
			client->SetBlockingMode(ov::BlockingMode::Blocking);
			client->SetConnectionTimeout(_timeout_msec);
			client->SetRequestHeader("X-OME-Signature", signature_sha1_base64);
			client->SetRequestHeader("Content-Type", "application/json");
			client->SetRequestHeader("Accept", "application/json");
			client->SetRequestBody(_message_body);

			ov::StopWatch watch;
			watch.Start();

			client->Request(_notification_server_url->ToUrlString(true), [=](http::StatusCode status_code, const std::shared_ptr<ov::Data> &data, const std::shared_ptr<const ov::Error> &error) {
				_elapsed_msec = watch.Elapsed();

				// A response was received from the server.
				if (error == nullptr)
				{
					if (status_code == http::StatusCode::OK)
					{
						// Parsing response
						ParseResponse(data);
						return;
					}
					else
					{
						SetStatus(StatusCode::INVALID_STATUS_CODE, ov::String::FormatString("Control server responded with %d status code.", static_cast<uint16_t>(status_code)));
						return;
					}
				}
				else
				{
					// A connection error or an error that does not conform to the HTTP spec has occurred.
					SetStatus(StatusCode::INTERNAL_ERROR, ov::String::FormatString("The HTTP client's request failed. (error code(%d) error message(%s)", error->GetCode(), error->GetMessage().CStr()));
					return;
				}
			});
		}
	}  // namespace alrt
}  // namespace mon