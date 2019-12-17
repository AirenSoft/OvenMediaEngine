//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#include "orchestrator.h"
#include "orchestrator_private.h"

#include <base/provider/provider.h>

bool Orchestrator::PrepareOriginMap(const cfg::Origins &origins)
{
	std::lock_guard<decltype(_origin_map_mutex)> lock_guard(_origin_map_mutex);

	// Create a mapping table for origins
	const auto &origin_list = origins.GetOriginList();

	_origin_list.clear();

	logtd("Origin map: (%zu items)", origin_list.size());

	for (auto origin : origin_list)
	{
		auto &pass = origin.GetPass();
		logtd("  > %s ", origin.GetLocation().CStr());

		for (auto url : pass.GetUrlList())
		{
			logtd("    - %s", url.GetUrl().CStr());
		}

		_origin_list.emplace_back(origin);
	}
}

bool Orchestrator::RegisterModule(const std::shared_ptr<OrchestratorModuleInterface> &module)
{
	if (module == nullptr)
	{
		return false;
	}

	auto type = module->GetModuleType();

	// Check if module exists
	std::lock_guard<__decltype(_modules_mutex)> lock_guard(_modules_mutex);

	for (auto &info : _modules)
	{
		if (info.module == module)
		{
			if (info.type == type)
			{
				logtw("Module %p (%s) is already registered", module.get(), GetOrchestratorModuleTypeName(type));
			}
			else
			{
				logtw("The module type was %s, but now %s", GetOrchestratorModuleTypeName(info.type), GetOrchestratorModuleTypeName(type));
			}

			OV_ASSERT2(false);
			return false;
		}
	}

	_modules.emplace_back(type, module);
	auto &list = _module_map[type];
	list.push_back(module);

	logtd("Module %p (%s) is registered", module.get(), GetOrchestratorModuleTypeName(type));

	return true;
}

bool Orchestrator::UnregisterModule(const std::shared_ptr<OrchestratorModuleInterface> &module)
{
	std::lock_guard<__decltype(_modules_mutex)> lock_guard(_modules_mutex);

	for (auto info = _modules.begin(); info != _modules.end(); ++info)
	{
		if (info->module == module)
		{
			_modules.erase(info);
			auto &list = _module_map[info->type];
			logtd("Module %p (%s) is unregistered", module.get(), GetOrchestratorModuleTypeName(info->type));
			return true;
		}
	}

	logtw("Module %p not found", module.get());
	OV_ASSERT2(false);

	return false;
}

bool Orchestrator::RequestPullStreamUsingMap(const ov::String &location)
{
	// Find the origin using the location
	for (auto &origin : _origin_list)
	{
		logtd("Trying to find the item that match location: %s", location.CStr());

		if (location.HasPrefix(origin.location))
		{
			// If the location has the prefix that configured in <Origins>, extract the remaining part
			// For example, if the settings is:
			//      <Origin>
			//      	<Location>/app/stream</Location>
			//      	<Pass>
			//      		<Url>ovt://origin.airensoft.com:9000/another_app/and_stream</Url>
			//      	</Pass>
			//      </Origin>
			// And when the location is "/app/stream_o",
			//
			// <Location>: /app/stream
			// location:   /app/stream_o
			//                        ~~ <= remaining part
			auto remaining_part = location.Substring(origin.location.GetLength());

			logtd("Found: location: %s, remaining_part: %s", origin.location.CStr(), remaining_part.CStr());

			for (auto url : origin.url_list)
			{
				// Append the remaining_part to the URL

				// For example,
				//    url:     ovt://origin.airensoft.com:9000/another_app/and_stream
				//    new_url: ovt://origin.airensoft.com:9000/another_app/and_stream_o
				//                                                                   ~~ <= remaining part
				url.Append(remaining_part);

				if (RequestPullStreamUsingScheme(url))
				{
					logtd("%s stream is requested successfully", location.CStr());
					return true;
				}

				logtd("Could not pull the stream for URL: %s. Attempting next URL...", url.CStr());
			}
		}
	}

	return false;
}

bool Orchestrator::RequestPullStreamUsingScheme(const ov::String &url)
{
	// Find a provider type using the scheme
	auto parsed_url = ov::Url::Parse(url.CStr());
	auto scheme = parsed_url->Scheme().LowerCaseString();
	ProviderType type;

	logtd("Obtaining ProviderType URL %s...", url.CStr());

	if (scheme == "rtmp")
	{
		type = ProviderType::Rtmp;
	}
	else if (scheme == "rtsp")
	{
		type = ProviderType::Rtsp;
	}
	else if (scheme == "ovt")
	{
		type = ProviderType::Ovt;
	}
	else
	{
		logte("Could not find a provider for scheme %s", scheme.CStr());
		return false;
	}

	// Find the provider
	std::shared_ptr<OrchestratorProviderModuleInterface> module = nullptr;

	for (auto info = _modules.begin(); info != _modules.end(); ++info)
	{
		if (info->type == OrchestratorModuleType::Provider)
		{
			auto module = std::dynamic_pointer_cast<OrchestratorProviderModuleInterface>(info->module);
			auto provider = std::dynamic_pointer_cast<pvd::Provider>(module);

			if (provider == nullptr)
			{
				logtd("Provider must inherit from pvd::Provider");
				OV_ASSERT2(provider != nullptr);
				continue;
			}

			if (provider->GetProviderType() == type)
			{
				logtd("Trying to request pull stream: %s to %p (%d)...", url.CStr(), module.get(), provider->GetProviderType());
				return module->PullStream(url);
			}
		}
	}

	logtw("Provider (%d) is not found for URL: %s", type, url.CStr());
	return false;
}

bool Orchestrator::RequestPullStream(const ov::String &url)
{
	// Extract a path from the url
	auto parsed_url = ov::Url::Parse(url.CStr());

	if (parsed_url == nullptr)
	{
		// There is no scheme in the URL
		std::lock_guard<__decltype(_origin_map_mutex)> lock_guard_for_origin_map(_origin_map_mutex);
		std::lock_guard<__decltype(_modules_mutex)> lock_guard_for_modules(_modules_mutex);
		return RequestPullStreamUsingMap(url);
	}

	// The URL has a scheme
	std::lock_guard<__decltype(_modules_mutex)> lock_guard(_modules_mutex);
	return RequestPullStreamUsingScheme(url);
}

bool Orchestrator::RequestPullStream(const ov::String &application, const ov::String &stream)
{
	return RequestPullStream(ov::String::FormatString("/%s/%s", application.CStr(), stream.CStr()));
}
