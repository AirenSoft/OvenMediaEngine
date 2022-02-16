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

	void GetRequestBody(
		const std::shared_ptr<http::svr::HttpConnection> &client,
		Json::Value *request_body)
	{
		ov::JsonObject json_object;
		auto error = json_object.Parse(client->GetRequest()->GetRequestBody());

		if (error != nullptr)
		{
			throw http::HttpError(http::StatusCode::BadRequest, error->What());
		}

		if (request_body != nullptr)
		{
			*request_body = json_object.GetJsonValue();
		}
	}

	void GetVirtualHostMetrics(
		const ov::MatchResult &match_result,
		std::shared_ptr<mon::HostMetrics> *vhost_metrics)
	{
		auto vhost_name_group = match_result.GetNamedGroup("vhost_name");

		if (vhost_name_group.IsValid() == false)
		{
			throw http::HttpError(
				http::StatusCode::InternalServerError,
				"Could not find the virtual host regex group");
		}

		auto vhost_name = vhost_name_group.GetValue();
		auto vhost = GetVirtualHost(vhost_name);

		if (vhost == nullptr)
		{
			throw http::HttpError(
				http::StatusCode::NotFound,
				"Could not find the virtual host: [%s]", vhost_name.CStr());
		}

		if (vhost_metrics != nullptr)
		{
			*vhost_metrics = vhost;
		}
	}

	void GetApplicationMetrics(
		const ov::MatchResult &match_result,
		const std::shared_ptr<mon::HostMetrics> &vhost_metrics,
		std::shared_ptr<mon::ApplicationMetrics> *app_metrics)
	{
		auto app_name_group = match_result.GetNamedGroup("app_name");

		if (app_name_group.IsValid() == false)
		{
			throw http::HttpError(
				http::StatusCode::InternalServerError,
				"Could not find the application regex group");
		}

		auto vhost_name = vhost_metrics->GetName();
		auto app_name = app_name_group.GetValue();
		auto app = GetApplication(vhost_metrics, app_name);

		if (app == nullptr)
		{
			throw http::HttpError(
				http::StatusCode::NotFound,
				"Could not find the application: [%s/%s]", vhost_name.CStr(), app_name.CStr());
		}

		if (app_metrics != nullptr)
		{
			*app_metrics = app;
		}
	}

	MAY_THROWS(HttpError)
	void GetStreamMetrics(
		const ov::MatchResult &match_result,
		const std::shared_ptr<mon::HostMetrics> &vhost_metrics,
		const std::shared_ptr<mon::ApplicationMetrics> &app_metrics,
		std::shared_ptr<mon::StreamMetrics> *stream_metrics,
		std::vector<std::shared_ptr<mon::StreamMetrics>> *output_streams)
	{
		auto stream_name_group = match_result.GetNamedGroup("stream_name");

		if (stream_name_group.IsValid() == false)
		{
			throw http::HttpError(
				http::StatusCode::InternalServerError,
				"Could not find the stream regex group");
		}

		auto vhost_name = vhost_metrics->GetName();
		auto app_name = app_metrics->GetName();
		auto stream_name = stream_name_group.GetValue();
		auto stream = GetStream(app_metrics, stream_name, output_streams);

		if (stream == nullptr)
		{
			throw http::HttpError(
				http::StatusCode::NotFound,
				"Could not find the stream: [%s/%s/%s]", vhost_name.CStr(), app_name.CStr(), stream_name.CStr());
		}

		if (stream_metrics != nullptr)
		{
			*stream_metrics = stream;
		}
	}

	void FillDefaultAppConfigValues(Json::Value &app_config)
	{
		// Setting up the default values
		if (app_config.isMember("providers") == false)
		{
			auto &providers = app_config["providers"];

			providers["rtmp"] = Json::objectValue;
			providers["mpegts"] = Json::objectValue;
		}

		if (app_config.isMember("publishers") == false)
		{
			auto &publishers = app_config["publishers"];

			publishers["hls"] = Json::objectValue;
			publishers["dash"] = Json::objectValue;
			publishers["llDash"] = Json::objectValue;
			publishers["webrtc"] = Json::objectValue;
		}

		if (app_config.isMember("outputProfiles") == false)
		{
			Json::Value output_profile(Json::objectValue);

			output_profile["name"] = "bypass";
			output_profile["outputStreamName"] = "${OriginStreamName}";

			Json::Value codec;

			codec["bypass"] = true;

			auto &encodes = output_profile["encodes"];
			encodes["videos"].append(codec);
			encodes["audios"].append(codec);

			codec = Json::objectValue;
			codec["codec"] = "opus";
			codec["bitrate"] = 128000;
			codec["samplerate"] = 48000;
			codec["channel"] = 2;
			encodes["audios"].append(codec);

			app_config["outputProfiles"]["outputProfile"].append(output_profile);
		}
	}

	void ThrowIfVirtualIsReadOnly(const cfg::vhost::VirtualHost &vhost_config)
	{
		if (vhost_config.IsReadOnly())
		{
			throw http::HttpError(http::StatusCode::Forbidden,
								  "The VirtualHost is read-only: [%s]",
								  vhost_config.GetName().CStr());
		}
	}

	void ThrowIfOrchestratorNotSucceeded(ocst::Result result, const char *action, const char *resource_name, const char *resource_path)
	{
		switch (result)
		{
			case ocst::Result::Failed:
				throw http::HttpError(http::StatusCode::BadRequest,
									  "Failed to %s the %s: [%s]", action, resource_name, resource_path);

			case ocst::Result::Succeeded:
				break;

			case ocst::Result::Exists:
				throw http::HttpError(http::StatusCode::Conflict,
									  "Could not %s the %s: [%s] already exists", action, resource_name, resource_path);

			case ocst::Result::NotExists:
				throw http::HttpError(http::StatusCode::NotFound,
									  "Could not %s the %s: [%s] not exists", action, resource_name, resource_path);
		}
	}
}  // namespace api
