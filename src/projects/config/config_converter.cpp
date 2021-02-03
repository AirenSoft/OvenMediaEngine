//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2021 AirenSoft. All rights reserved.
//
//==============================================================================
#include "config_converter.h"

#include <monitoring/monitoring.h>

#include "config_private.h"

namespace cfg
{
	namespace conv
	{
		Json::Value GetMpegTsStreamListFromMetrics(Json::Value providers, const std::map<uint32_t, std::shared_ptr<ReservedStreamMetrics>> &reserved_stream_list)
		{
			if (reserved_stream_list.size() == 0)
			{
				// Nothing to do
				return Json::nullValue;
			}

			auto &mpeg_ts_provider = providers["mpegts"];

			// Reset stream list (Currently, streams contains the settings that were loaded at the beginning of the OME run)
			auto &streams = mpeg_ts_provider["streams"];
			streams = Json::arrayValue;

			for (auto &key : reserved_stream_list)
			{
				Json::Value stream;

				auto &reserved_stream = key.second;
				auto &uri = reserved_stream->GetStreamUri();

				auto name = reserved_stream->GetReservedStreamName();
				stream["name"] = name.CStr();

				auto port = ov::String::FormatString("%d/%s", uri.Port(), uri.Scheme().LowerCaseString().CStr());
				stream["port"] = port.CStr();

				streams.append(stream);
			}

			return providers;
		}

		Json::Value GetApplicationFromMetrics(const std::shared_ptr<const mon::ApplicationMetrics> &app_metrics)
		{
			auto app = app_metrics->GetConfig().ToJson();
			auto reserved_stream_list = app_metrics->GetReservedStreamMetricsMap();

			if (reserved_stream_list.size() > 0)
			{
				app["providers"] = GetMpegTsStreamListFromMetrics(app["providers"], reserved_stream_list);
			}

			return std::move(app);
		}

		Json::Value GetApplicationListFromMetrics(const std::map<uint32_t, std::shared_ptr<mon::ApplicationMetrics>> &app_metrics_list, bool include_dynamic_app)
		{
			Json::Value apps = Json::arrayValue;

			for (auto &app_pair : app_metrics_list)
			{
				auto &app_metrics = app_pair.second;

				if ((include_dynamic_app == false) && app_metrics->IsDynamicApp())
				{
					// Don't save the dynamic app
					continue;
				}

				apps.append(GetApplicationFromMetrics(app_metrics));
			}

			return std::move(apps);
		}

		Json::Value GetVirtualHostFromMetrics(const std::shared_ptr<mon::HostMetrics> &vhost_metrics, bool include_dynamic_app)
		{
			Json::Value vhost = std::static_pointer_cast<cfg::vhost::VirtualHost>(vhost_metrics)->ToJson();

			// Reset application list (Currently, vhost contains the settings that were loaded at the beginning of the OME run)
			vhost["applications"] = GetApplicationListFromMetrics(vhost_metrics->GetApplicationMetricsList(), include_dynamic_app);

			return std::move(vhost);
		}

		Json::Value GetVirtualHostListFromMetrics(Json::Value server_config, const std::map<uint32_t, std::shared_ptr<mon::HostMetrics>> &vhost_metrics_list, bool include_dynamic_app)
		{
			if (vhost_metrics_list.size() > 0)
			{
				// Save the VirtualHost list

				// Reset virtual host list (Currently, server_config contains the settings that were loaded at the beginning of the OME run)
				Json::Value &vhosts = server_config["virtualHosts"];
				vhosts = Json::arrayValue;

				for (auto vhost_metrics_pair : vhost_metrics_list)
				{
					vhosts.append(GetVirtualHostFromMetrics(vhost_metrics_pair.second, include_dynamic_app));
				}
			}
			else
			{
				if (server_config.isMember("virtualHosts") == false)
				{
					logte("The values of ConfigManager and Monitoring do not match even though the VirtualHost list is immutable. (VirtualHost is not loaded on Monitoring?)");
					OV_ASSERT2(false);

					return Json::nullValue;
				}

				// Nothing to do
			}

			return std::move(server_config);
		}

		Json::Value GetServerJsonFromConfig(const std::shared_ptr<cfg::Server> &server_config, bool include_dynamic_app)
		{
			// Convert server config to JSON to obtain <Bind> and etc
			return conv::GetVirtualHostListFromMetrics(server_config->ToJson(), mon::Monitoring::GetInstance()->GetHostMetricsList(), include_dynamic_app);
		}

	}  // namespace conv
}  // namespace cfg
