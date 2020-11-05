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
		: scheme(origin_config.GetPass().GetScheme()),
		  location(origin_config.GetLocation()),
		  state(ItemState::New)

	{
		for (auto &item : origin_config.GetPass().GetUrlList())
		{
			auto url = item.GetUrl();

			// Prepend "<scheme>://"
			url.Prepend("://");
			url.Prepend(scheme);

			url_list.push_back(item.GetUrl());
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
	}

	//--------------------------------------------------------------------
	// Implementation of MediaRouteApplicationObserver
	//--------------------------------------------------------------------
	// Temporarily used until Orchestrator takes stream management
	bool Application::OnCreateStream(const std::shared_ptr<info::Stream> &info)
	{
		return callback->OnCreateStream(app_info, info);
	}

	bool Application::OnDeleteStream(const std::shared_ptr<info::Stream> &info)
	{
		return callback->OnDeleteStream(app_info, info);
	}

	bool Application::OnSendFrame(const std::shared_ptr<info::Stream> &info, const std::shared_ptr<MediaPacket> &packet)
	{
		// Ignore packets
		return true;
	}

	MediaRouteApplicationObserver::ObserverType Application::GetObserverType()
	{
		return ObserverType::Orchestrator;
	}

	//--------------------------------------------------------------------
	// ocst::VirtualHost
	//--------------------------------------------------------------------
	VirtualHost::VirtualHost(const info::Host &host_info)
		: host_info(host_info), state(ItemState::New)

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

	bool VirtualHost::MarkAllAs(ItemState expected_old_state, ItemState state)
	{
		if (this->state != expected_old_state)
		{
			return false;
		}

		this->state = state;

		for (auto &host : host_list)
		{
			if (host.state != expected_old_state)
			{
				return false;
			}

			host.state = state;
		}

		for (auto &origin : origin_list)
		{
			if (origin.state != expected_old_state)
			{
				return false;
			}

			origin.state = state;
		}

		return true;
	}
}  // namespace ocst
