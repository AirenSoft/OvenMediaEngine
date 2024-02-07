//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#include "structures.h"

namespace ocst
{
	//--------------------------------------------------------------------
	// ocst::Module
	//--------------------------------------------------------------------
	Module::Module(ModuleType type, const std::shared_ptr<ModuleInterface> &module)
		: type(type),
		  module(module)
	{
	}

	//--------------------------------------------------------------------
	// ocst::Stream
	//--------------------------------------------------------------------
	Stream::Stream(const info::Application &app_info, const std::shared_ptr<PullProviderModuleInterface> &provider, const std::shared_ptr<pvd::Stream> &provider_stream, const ov::String &full_name)
		: app_info(app_info),
		  provider(provider),
		  provider_stream(provider_stream),
		  full_name(full_name)
	{
		is_valid = true;
	}

	//--------------------------------------------------------------------
	// ocst::Origin
	//--------------------------------------------------------------------
	Origin::Origin(const cfg::vhost::orgn::Origin &origin_config)
		: state(ItemState::New)
	{
		scheme = origin_config.GetPass().GetScheme();
		location = origin_config.GetLocation();
		forward_query_params = origin_config.GetPass().IsForwardQueryParamsEnabled();

		bool parsed = false;
		persistent = origin_config.IsPersistent();
		failback = origin_config.IsFailback();
		strict_location = origin_config.IsStrictLocation();
		ignore_rtcp_sr_timestamp = origin_config.IsRtcpSrTimestampIgnored();
		relay = origin_config.IsRelay(&parsed);
		if (parsed == false && scheme.UpperCaseString() == "OVT")
		{
			relay = true;
		}

		for (auto &url : origin_config.GetPass().GetUrlList())
		{
			url_list.push_back(url);
		}

		this->origin_config = origin_config;
	}

	bool Origin::IsValid() const
	{
		return state != ItemState::Unknown;
	}

	//--------------------------------------------------------------------
	// ocst::Host
	//--------------------------------------------------------------------
	Host::Host(const ov::String &name)
		: name(name),
		  state(ItemState::New)
	{
		UpdateRegex();
	}

	bool Host::IsValid() const
	{
		return state != ItemState::Unknown;
	}

	bool Host::UpdateRegex()
	{
		// Escape special characters: '[', '\', '.', '/', '+', '{', '}', '$', '^', '|' to \<char>
		auto special_characters = std::regex(R"([[\\.\/+{}$^|])");
		ov::String escaped = std::regex_replace(name.CStr(), special_characters, R"(\$&)").c_str();
		// Change '*'/'?' to .<char>
		escaped = escaped.Replace(R"(*)", R"(.*)");
		escaped = escaped.Replace(R"(?)", R"(.?)");
		escaped.Prepend("^");
		escaped.Append("$");

		std::regex expression;

		try
		{
			regex_for_domain = std::regex(escaped);
		}
		catch (std::exception &e)
		{
			return false;
		}

		return true;
	}

	//--------------------------------------------------------------------
	// Application
	//--------------------------------------------------------------------
	Application::Application(CallbackInterface *callback, const info::Application &app_info)
		: callback(callback),
		  app_info(app_info)
	{
		idle_timer.Start();
	}

	bool Application::IsUnusedFor(int seconds) const
	{
		return idle_timer.IsElapsed(seconds * 1000);
	}

	//--------------------------------------------------------------------
	// Implementation of MediaRouterApplicationObserver
	//--------------------------------------------------------------------
	// Temporarily used until Orchestrator takes stream management
	bool Application::OnStreamCreated(const std::shared_ptr<info::Stream> &info)
	{
		logd("DEBUG", "%s stream has been created: %s/%s", info->IsInputStream()?"Inbound":"Outbound", app_info.GetName().CStr(), info->GetName().CStr());

		if (info->IsInputStream())
		{
			provider_stream_map[info->GetName()] = std::static_pointer_cast<pvd::Stream>(info);
		}
		else
		{
			publisher_stream_map[info->GetName()] = std::static_pointer_cast<pub::Stream>(info);
		}

		idle_timer.Stop();

		return callback->OnStreamCreated(app_info, info);
	}

	bool Application::OnStreamDeleted(const std::shared_ptr<info::Stream> &info)
	{
		logd("DEBUG", "%s stream has been deleted: %s/%s", info->IsInputStream()?"Inbound":"Outbound", app_info.GetName().CStr(), info->GetName().CStr());

		if (info->IsInputStream())
		{
			provider_stream_map.erase(info->GetName());
		}
		else
		{
			publisher_stream_map.erase(info->GetName());
		}

		if (provider_stream_map.empty() && publisher_stream_map.empty())
		{
			idle_timer.Start();
		}
	
		return callback->OnStreamDeleted(app_info, info);
	}

	std::shared_ptr<pvd::Stream> Application::GetProviderStream(const ov::String &stream_name)
	{
		auto item = provider_stream_map.find(stream_name);

		if (item == provider_stream_map.end())
		{
			return nullptr;
		}

		return item->second;
	}
	
	std::shared_ptr<pub::Stream> Application::GetPublisherStream(const ov::String &stream_name)
	{
		auto item = publisher_stream_map.find(stream_name);

		if (item == publisher_stream_map.end())
		{
			return nullptr;
		}

		return item->second;
	}

	size_t Application::GetProviderStreamCount() const
	{
		return provider_stream_map.size();
	}

	size_t Application::GetPublisherStreamCount() const
	{
		return publisher_stream_map.size();
	}

	bool Application::OnStreamPrepared(const std::shared_ptr<info::Stream> &info)
	{
		return callback->OnStreamPrepared(app_info, info);
	}

	bool Application::OnStreamUpdated(const std::shared_ptr<info::Stream> &info)
	{
		return callback->OnStreamUpdated(app_info, info);
	}

	bool Application::OnSendFrame(const std::shared_ptr<info::Stream> &info, const std::shared_ptr<MediaPacket> &packet)
	{
		// Ignore packets
		return true;
	}

	MediaRouterApplicationObserver::ObserverType Application::GetObserverType()
	{
		return ObserverType::Orchestrator;
	}

	//--------------------------------------------------------------------
	// ocst::VirtualHost
	//--------------------------------------------------------------------
	VirtualHost::VirtualHost(const info::Host &new_host_info)
		: host_info(new_host_info), state(ItemState::New)

	{
	}

	void VirtualHost::MarkAllAs(ItemState state)
	{
		this->state = state;

		for (auto &host : host_list)
		{
			host.state = state;
		}

		for (auto &origin : origin_list)
		{
			origin.state = state;
		}
	}

	bool VirtualHost::MarkAllAs(ItemState state, int state_count, ...)
	{
		va_list list;
		va_start(list, state_count);

		std::map<ItemState, bool> expected_state_map;

		for (int index = 0; index < state_count; index++)
		{
			expected_state_map[va_arg(list, ItemState)] = true;
		}
		va_end(list);

		if (expected_state_map.find(this->state) == expected_state_map.end())
		{
			return false;
		}

		this->state = state;

		for (auto &host : host_list)
		{
			if (expected_state_map.find(host.state) == expected_state_map.end())
			{
				return false;
			}

			host.state = state;
		}

		for (auto &origin : origin_list)
		{
			if (expected_state_map.find(origin.state) == expected_state_map.end())
			{
				return false;
			}

			origin.state = state;
		}

		return true;
	}
}  // namespace ocst
