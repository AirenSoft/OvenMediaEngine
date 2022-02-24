#include "admission_webhooks.h"

#include <modules/http/client/http_client.h>

std::shared_ptr<AdmissionWebhooks> AdmissionWebhooks::Query(ProviderType provider,
															const std::shared_ptr<ov::Url> &control_server_url, uint32_t timeout_msec,
															const ov::String secret_key,
															const std::shared_ptr<ov::SocketAddress> &client_address,
															const std::shared_ptr<const ov::Url> &request_url,
															const Status::Code status)
{
	auto hooks = std::make_shared<AdmissionWebhooks>();

	hooks->_provider_type = provider;
	hooks->_control_server_url = control_server_url;
	hooks->_timeout_msec = timeout_msec;
	hooks->_secret_key = secret_key;
	hooks->_client_address = client_address;
	hooks->_requested_url = request_url;
	hooks->_status = status;

	hooks->Run();

	return hooks;
}

std::shared_ptr<AdmissionWebhooks> AdmissionWebhooks::Query(PublisherType publisher,
															const std::shared_ptr<ov::Url> &control_server_url, uint32_t timeout_msec,
															const ov::String secret_key,
															const std::shared_ptr<ov::SocketAddress> &client_address,
															const std::shared_ptr<const ov::Url> &request_url,
															const Status::Code status)
{
	auto hooks = std::make_shared<AdmissionWebhooks>();

	hooks->_publisher_type = publisher;
	hooks->_control_server_url = control_server_url;
	hooks->_timeout_msec = timeout_msec;
	hooks->_secret_key = secret_key;
	hooks->_client_address = client_address;
	hooks->_requested_url = request_url;
	hooks->_status = status;

	hooks->Run();

	return hooks;
}

AdmissionWebhooks::ErrCode AdmissionWebhooks::GetErrCode() const
{
	return _err_code;
}

ov::String AdmissionWebhooks::GetErrReason() const
{
	return _err_reason;
}

std::shared_ptr<ov::Url> AdmissionWebhooks::GetNewURL() const
{
	return _new_url;
}

uint64_t AdmissionWebhooks::GetLifetime() const
{
	return _lifetime;
}

uint64_t AdmissionWebhooks::GetElpasedTime() const
{
	return _elapsed_ms;
}

void AdmissionWebhooks::SetError(ErrCode code, ov::String reason)
{
	_err_code = code;
	_err_reason = reason;
}

ov::String AdmissionWebhooks::GetMessageBody()
{
	/*
	{
		"client": 
		{
			"address": "211.233.58.86",
			"port": 29291
		},
		"request":
		{
			"direction": "incoming | outgoing",
			"protocol": "webrtc | rtmp | srt | hls | dash | lldash",
			"url": "scheme://host[:port]/app/stream/file?query=value&query2=value2",
			"status": "opening | closing",
			"time": "2021-05-12T13:45:00.000Z" // ISO8601
		}
	}
	*/
	// Make request message
	Json::Value jv_root;
	Json::Value jv_client;
	Json::Value jv_request;

	if(_client_address != nullptr)
	{
		jv_client["address"] = _client_address->GetIpAddress().CStr();
		jv_client["port"] = _client_address->Port();
		jv_root["client"] = jv_client;
	}

	ov::String direction, protocol;
	if(_provider_type != ProviderType::Unknown)
	{
		direction = "incoming";
		protocol = StringFromProviderType(_provider_type);
	}
	else if(_publisher_type != PublisherType::Unknown)
	{
		direction = "outgoing";
		protocol = StringFromPublisherType(_publisher_type);
	}
	else
	{
		// error
		SetError(ErrCode::INTERNAL_ERROR, "Invalid parameters");
		return "";
	}

	jv_request["direction"] = direction.CStr();
	jv_request["protocol"] = protocol.CStr();
	jv_request["url"] = _requested_url->ToUrlString(true).CStr();
	jv_request["status"] = Status::Description(_status).CStr();
	jv_request["time"] = ov::Converter::ToISO8601String(std::chrono::system_clock::now()).CStr();
	jv_root["request"] = jv_request;

	return ov::Converter::ToString(jv_root);
}

void AdmissionWebhooks::ParseResponse(const std::shared_ptr<ov::Data> &data)
{
	ov::JsonObject object = ov::Json::Parse(data->ToString());
	if(object.IsNull())
	{
		SetError(ErrCode::INVALID_DATA_FORMAT, ov::String::FormatString("Json parsing error : a response in the wrong format was received."));
		return;
	}

	if (_status == Status::Code::CLOSING)
	{
		SetError(ErrCode::ALLOWED, _err_reason);
		return;
	}

	/*
	Required : "allowed"
	Optional : "new_url", "lifetime", "reason"

	{
		"allowed": true,
		"new_url": "scheme://host[:port]/app/stream/file?query=value&query2=value2",
		"lifetime": seconds   // 0 : infinite
	}

	{
		"allowed": false,
		"reason": "optional value"
	}
	*/

	// Required data
	Json::Value &jv_allowed = object.GetJsonValue()["allowed"];
	if(jv_allowed.isNull() || jv_allowed.isBool() == false)
	{
		SetError(ErrCode::INVALID_DATA_FORMAT, ov::String::FormatString("Json parsing error : In response, \"allowed\" is required data and must be of type bool."));
		return;
	}

	_allowed = jv_allowed.asBool();
	if(_allowed == false)
	{
		_err_reason.Format("ControlServer(%s) denied admission to %s by %s.", _control_server_url->ToUrlString().CStr(), _requested_url->ToUrlString().CStr(), _client_address->ToString(false).CStr());
	}

	// Optional data
	Json::Value &jv_new_url = object.GetJsonValue()["new_url"];
	Json::Value &jv_lifetime = object.GetJsonValue()["lifetime"];
	Json::Value &jv_reason = object.GetJsonValue()["reason"];

	if(jv_new_url.isNull() == false)
	{
		if(jv_new_url.isString())
		{
			_new_url = ov::Url::Parse(jv_new_url.asString().c_str());
		}
	}

	if(jv_lifetime.isNull() == false)
	{
		if(jv_lifetime.isUInt64())
		{
			_lifetime = jv_lifetime.asUInt64();
		}
	}

	if(jv_reason.isNull() == false)
	{
		if(jv_reason.isString())
		{
			_err_reason = jv_reason.asString().c_str();
		}
	}

	SetError(_allowed ? ErrCode::ALLOWED : ErrCode::DENIED, _err_reason);
}

void AdmissionWebhooks::Run()
{
	auto body = GetMessageBody();
	if(body.IsEmpty())
	{
		// Error
		return;
	}

	// Set X-OME-Signature
	auto md_sha1 = ov::MessageDigest::ComputeHmac(ov::CryptoAlgorithm::Sha1, _secret_key.ToData(false), body.ToData(false));
	if(md_sha1 == nullptr)
	{
		// Error
		SetError(ErrCode::INTERNAL_ERROR, ov::String::FormatString("Signature creation failed.(Method : HMAC(SHA1), Key : %s, Body length : %d", _secret_key.CStr(), body.GetLength()));
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

	client->Request(_control_server_url->ToUrlString(true), [=](http::StatusCode status_code, const std::shared_ptr<ov::Data> &data, const std::shared_ptr<const ov::Error> &error) 
	{
		_elapsed_ms = watch.Elapsed();

		// A response was received from the server.
		if(error == nullptr) 
		{	
			if(status_code == http::StatusCode::OK) 
			{
				// Parsing response
				ParseResponse(data);
				return;
			} 
			else 
			{
				SetError(ErrCode::INVALID_STATUS_CODE, ov::String::FormatString("Control server responded with %d status code.", static_cast<uint16_t>(status_code)));
				return;
			}
		}
		else
		{
			// A connection error or an error that does not conform to the HTTP spec has occurred.
			SetError(ErrCode::INTERNAL_ERROR, ov::String::FormatString("The HTTP client's request failed. (error code(%d) error message(%s)", error->GetCode(), error->GetMessage().CStr()));
			return;
		}
	});
}
