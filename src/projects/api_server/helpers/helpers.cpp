//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#include "./helpers.h"

#include <modules/http/http.h>

namespace api
{
	std::map<uint32_t, std::shared_ptr<mon::HostMetrics>> GetVirtualHostList()
	{
		return mon::Monitoring::GetInstance()->GetHostMetricsList();
	}

	std::shared_ptr<mon::HostMetrics> GetVirtualHost(const ov::String &vhost_name)
	{
		auto vhost_list = GetVirtualHostList();

		for (const auto &vhost : vhost_list)
		{
			if (vhost_name == vhost.second->GetName().CStr())
			{
				return vhost.second;
			}
		}

		return nullptr;
	}

	std::map<uint32_t, std::shared_ptr<mon::ApplicationMetrics>> GetApplicationList(const std::shared_ptr<mon::HostMetrics> &vhost)
	{
		return vhost->GetApplicationMetricsList();
	}

	std::shared_ptr<mon::ApplicationMetrics> GetApplication(const std::shared_ptr<mon::HostMetrics> &vhost, const ov::String &app_name)
	{
		if (vhost != nullptr)
		{
			auto app_list = GetApplicationList(vhost);
			for (auto &item : app_list)
			{
				if (app_name == item.second->GetName().GetAppName().CStr())
				{
					return item.second;
				}
			}
		}

		return nullptr;
	}

	std::map<uint32_t, std::shared_ptr<mon::StreamMetrics>> GetStreamList(const std::shared_ptr<mon::ApplicationMetrics> &application)
	{
		return application->GetStreamMetricsMap();
	}

	std::shared_ptr<mon::StreamMetrics> GetStream(const std::shared_ptr<mon::ApplicationMetrics> &application, const ov::String &stream_name, std::vector<std::shared_ptr<mon::StreamMetrics>> *output_streams)
	{
		if (application != nullptr)
		{
			auto stream_list = GetStreamList(application);

			std::shared_ptr<mon::StreamMetrics> input_stream;

			for (auto &item : stream_list)
			{
				auto &stream = item.second;

				if (input_stream == nullptr)
				{
					if (
						// Stream is an input stream
						(stream->GetLinkedInputStream() == nullptr) &&
						(stream_name == stream->GetName().CStr()))
					{
						input_stream = item.second;

						if (output_streams == nullptr)
						{
							break;
						}
						else
						{
							// Continue to find the stream associated with input_stream
						}
					}
				}
				else
				{
					auto &origin_stream = stream->GetLinkedInputStream();

					if ((origin_stream != nullptr) && (origin_stream->GetId() == input_stream->GetId()))
					{
						output_streams->push_back(item.second);
					}
				}
			}

			return input_stream;
		}

		return nullptr;
	}

	std::shared_ptr<const http::HttpError> GetRequestBody(
		const std::shared_ptr<http::svr::HttpConnection> &client,
		Json::Value *request_body)
	{
		ov::JsonObject json_object;
		auto error = http::HttpError::CreateError(json_object.Parse(client->GetRequest()->GetRequestBody()));

		if (error != nullptr)
		{
			return error;
		}

		if (request_body != nullptr)
		{
			*request_body = json_object.GetJsonValue();
		}

		return nullptr;
	}

	std::shared_ptr<const http::HttpError> GetVirtualHostMetrics(
		const ov::MatchResult &match_result,
		std::shared_ptr<mon::HostMetrics> *vhost_metrics)
	{
		auto vhost_name_group = match_result.GetNamedGroup("vhost_name");

		if (vhost_name_group.IsValid())
		{
			auto vhost_name = vhost_name_group.GetValue();
			auto vhost = GetVirtualHost(vhost_name);

			if (vhost == nullptr)
			{
				return http::HttpError::CreateError(
					http::StatusCode::NotFound,
					"Could not find the virtual host: [%s]", vhost_name.CStr());
			}

			if (vhost_metrics != nullptr)
			{
				*vhost_metrics = vhost;
			}

			return nullptr;
		}

		return http::HttpError::CreateError(
			http::StatusCode::InternalServerError,
			"Could not find the virtual host regex group");
	}

	std::shared_ptr<const http::HttpError> GetApplicationMetrics(
		const ov::MatchResult &match_result,
		const std::shared_ptr<mon::HostMetrics> &vhost_metrics,
		std::shared_ptr<mon::ApplicationMetrics> *app_metrics)
	{
		auto app_name_group = match_result.GetNamedGroup("app_name");

		if (app_name_group.IsValid())
		{
			auto vhost_name = vhost_metrics->GetName();
			auto app_name = app_name_group.GetValue();
			auto app = GetApplication(vhost_metrics, app_name);

			if (app == nullptr)
			{
				return http::HttpError::CreateError(
					http::StatusCode::NotFound,
					"Could not find the application: [%s/%s]", vhost_name.CStr(), app_name.CStr());
			}

			if (app_metrics != nullptr)
			{
				*app_metrics = app;
			}

			return nullptr;
		}

		return http::HttpError::CreateError(
			http::StatusCode::InternalServerError,
			"Could not find the application regex group");
	}

	std::shared_ptr<const http::HttpError> GetStreamMetrics(
		const ov::MatchResult &match_result,
		const std::shared_ptr<mon::HostMetrics> &vhost_metrics,
		const std::shared_ptr<mon::ApplicationMetrics> &app_metrics,
		std::shared_ptr<mon::StreamMetrics> *stream_metrics,
		std::vector<std::shared_ptr<mon::StreamMetrics>> *output_streams)
	{
		auto stream_name_group = match_result.GetNamedGroup("stream_name");

		if (stream_name_group.IsValid())
		{
			auto vhost_name = vhost_metrics->GetName();
			auto app_name = app_metrics->GetName();
			auto stream_name = stream_name_group.GetValue();
			auto stream = GetStream(app_metrics, stream_name, output_streams);

			if (stream == nullptr)
			{
				return http::HttpError::CreateError(
					http::StatusCode::NotFound,
					"Could not find the stream: [%s/%s/%s]", vhost_name.CStr(), app_name.CStr(), stream_name.CStr());
			}

			if (stream_metrics != nullptr)
			{
				*stream_metrics = stream;
			}

			return nullptr;
		}

		return http::HttpError::CreateError(
			http::StatusCode::InternalServerError,
			"Could not find the stream regex group");
	}

	void MultipleStatus::AddStatusCode(http::StatusCode status_code)
	{
		if (_count == 0)
		{
			_last_status_code = status_code;
			_has_ok = (status_code == http::StatusCode::OK);
		}
		else
		{
			_has_ok = _has_ok || (status_code == http::StatusCode::OK);

			if (_last_status_code != status_code)
			{
				_last_status_code = http::StatusCode::MultiStatus;
			}
			else
			{
				// Keep the status code
			}
		}

		_count++;
	}

	void MultipleStatus::AddStatusCode(const std::shared_ptr<const ov::Error> &error)
	{
		http::StatusCode error_status_code = static_cast<http::StatusCode>(error->GetCode());

		AddStatusCode(IsValidStatusCode(error_status_code) ? error_status_code : http::StatusCode::InternalServerError);
	}

	http::StatusCode MultipleStatus::GetStatusCode() const
	{
		return _last_status_code;
	}
}  // namespace api
