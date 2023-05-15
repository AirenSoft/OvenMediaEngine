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
		std::shared_ptr<Notification> Notification::Query(const std::shared_ptr<ov::Url> &notification_server_url, uint32_t timeout_msec, const ov::String secret_key,
															const ov::String &source_uri, const std::shared_ptr<std::vector<std::shared_ptr<Message>>> &message_list,
															const std::shared_ptr<StreamMetrics> &stream_metric)
		{
			auto notification = std::make_shared<Notification>();

			notification->_notification_server_url = notification_server_url;
			notification->_timeout_msec = timeout_msec;
			notification->_secret_key = secret_key;
			notification->_source_uri = source_uri;
			notification->_message_list = message_list;
			notification->_stream_metric = stream_metric;
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
			// Make request message
			Json::Value jv_root;

			// Messages
			Json::Value jv_messages;
			if (_message_list == nullptr || _message_list->size() <= 0)
			{
				// If the message_list is empty, it means that the status of the stream has become normal, so send an OK message.

				Json::Value jv_message;

				jv_message["code"] = Message::StringFromMessageCode(Message::Code::OK).CStr();
				jv_message["description"] = Message::DescriptionFromMessageCode<bool>(Message::Code::OK, true, true).CStr();

				jv_messages.append(jv_message);
			}
			else
			{
				for (auto &message : *_message_list)
				{
					Json::Value jv_message;

					jv_message["code"] = Message::StringFromMessageCode(message->GetCode()).CStr();
					jv_message["description"] = message->GetDescription().CStr();

					jv_messages.append(jv_message);
				}
			}

			// Metric infos
			Json::Value jv_source_info = ::serdes::JsonFromStream(_stream_metric);

			jv_root["sourceUri"] = _source_uri.CStr();
			jv_root["messages"] = jv_messages;
			jv_root["sourceInfo"] = jv_source_info;

			return ov::Converter::ToString(jv_root);
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
			auto body = GetMessageBody();
			if (body.IsEmpty())
			{
				// Error
				return;
			}

			// Set X-OME-Signature
			auto md_sha1 = ov::MessageDigest::ComputeHmac(ov::CryptoAlgorithm::Sha1, _secret_key.ToData(false), body.ToData(false));
			if (md_sha1 == nullptr)
			{
				// Error
				SetStatus(StatusCode::INTERNAL_ERROR, ov::String::FormatString("Signature creation failed.(Method : HMAC(SHA1), Body length : %d", body.GetLength()));
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
			client->SetRequestBody(body);

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