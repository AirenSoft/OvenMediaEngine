//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#include "./helpers.h"

namespace api
{
	std::map<uint32_t, std::shared_ptr<mon::HostMetrics>> GetVirtualHostList()
	{
		return std::move(mon::Monitoring::GetInstance()->GetHostMetricsList());
	}

	std::shared_ptr<mon::HostMetrics> GetVirtualHost(const std::string_view &vhost_name)
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
		return std::move(vhost->GetApplicationMetricsList());
	}

	std::shared_ptr<mon::ApplicationMetrics> GetApplication(const std::shared_ptr<mon::HostMetrics> &vhost, const std::string_view &app_name)
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
		return std::move(application->GetStreamMetricsMap());
	}

	std::shared_ptr<mon::StreamMetrics> GetStream(const std::shared_ptr<mon::ApplicationMetrics> &application, const std::string_view &stream_name, std::vector<std::shared_ptr<mon::StreamMetrics>> *output_streams)
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
						(stream->GetOriginStream() == nullptr) &&
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
					auto &origin_stream = stream->GetOriginStream();

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

	void MultipleStatus::AddStatusCode(HttpStatusCode status_code)
	{
		if (_count == 0)
		{
			_last_status_code = status_code;
			_has_ok = (status_code == HttpStatusCode::OK);
		}
		else
		{
			_has_ok = _has_ok || (status_code == HttpStatusCode::OK);

			if (_last_status_code != status_code)
			{
				_last_status_code = HttpStatusCode::MultiStatus;
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
		HttpStatusCode error_status_code = static_cast<HttpStatusCode>(error->GetCode());

		AddStatusCode(IsValidHttpStatusCode(error_status_code) ? error_status_code : HttpStatusCode::InternalServerError);
	}

	HttpStatusCode MultipleStatus::GetStatusCode() const
	{
		return _last_status_code;
	}
}  // namespace api
