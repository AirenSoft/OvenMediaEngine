//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "orchestrator_internal.h"

namespace ocst
{
	//
	// Orchestrator is responsible for passing commands to registered modules, such as Provider/MediaRouter/Transcoder/Publisher.
	//
	// Orchestrator will upgrade to perform the following roles:
	//
	// 1. The publisher can request the provider to create a stream.
	// 2. Other modules may request Provider/Publisher traffic information. (Especially, it will be used by the RESTful API server)
	// 3. Create or manage new applications.
	//    For example, if some module calls Orchestrator::CreateApplication(), the Orchestrator will create a new app
	//    using the APIs of Providers, MediaRouter, and Publishers as appropriate.
	//
	// TODO(dimiden): Modification is required so that the module can be managed per Host
	class Orchestrator : public ov::Singleton<Orchestrator>,
						 protected OrchestratorInternal
	{
	public:
		bool ApplyOriginMap(const std::vector<info::Host> &host_list);

		std::vector<std::shared_ptr<ocst::VirtualHost>> GetVirtualHostList();

		/// Register the module
		///
		/// @param module Module to register
		///
		/// @return If the module is registered or passed a different type from the previously registered type, false is returned.
		/// Otherwise, true is returned.
		bool RegisterModule(const std::shared_ptr<ModuleInterface> &module);

		/// Unregister the module
		///
		/// @param module Module to unregister
		///
		/// @return If the module is not already registered, false is returned. Otherwise, true is returned.
		bool UnregisterModule(const std::shared_ptr<ModuleInterface> &module);

		ov::String GetVhostNameFromDomain(const ov::String &domain_name) const;

		/// Generate an application name for vhost/app
		///
		/// @param vhost_name A name of VirtualHost
		/// @param app_name An application name
		///
		/// @return A new application name corresponding to vhost/app
		info::VHostAppName ResolveApplicationName(const ov::String &vhost_name, const ov::String &app_name) const override
		{
			return OrchestratorInternal::ResolveApplicationName(vhost_name, app_name);
		}

		///  Generate an application name for domain/app
		///
		/// @param domain_name A name of the domain
		/// @param app_name An application name
		///
		/// @return A new application name corresponding to domain/app
		info::VHostAppName ResolveApplicationNameFromDomain(const ov::String &domain_name, const ov::String &app_name) const;

		bool GetUrlListForLocation(const info::VHostAppName &vhost_app_name, const ov::String &host_name, const ov::String &stream_name, std::vector<ov::String> *url_list);

		/// Create an application and notify the modules
		///
		/// @param vhost_name A name of VirtualHost
		/// @param app_config Application configuration to create
		///
		/// @return Creation result
		///
		/// @note Automatically DeleteApplication() when application creation fails
		Result CreateApplication(const info::Host &vhost_info, const cfg::vhost::app::Application &app_config);
		/// Delete the application and notify the modules
		///
		/// @param app_info Application information to delete
		///
		/// @return
		///
		/// @note If an error occurs during deletion, do not recreate the application
		Result DeleteApplication(const info::Application &app_info);

		Result Release();

		const info::Application &GetApplicationInfo(const ov::String &vhost_name, const ov::String &app_name) const;
		const info::Application &GetApplicationInfo(const info::VHostAppName &vhost_app_name) const;

		/// Pull a stream using specified URL with offset
		///
		/// @param request_from Source from which RequestPullStream() invoked (Mainly provided when requested by Publisher)
		/// @param vhost_app_name When the URL is pulled, its stream is created in this vhost_name and app_name
		/// @param stream_name When the URL is pulled, its stream is created in this stream_name
		/// @param url URL to pull
		/// @param offset Parameters to be used when you want to pull from a certain point (available only when the provider supports that)
		bool RequestPullStream(
			const std::shared_ptr<const ov::Url> &request_from,
			const info::VHostAppName &vhost_app_name, const ov::String &stream_name,
			const ov::String &url, off_t offset);

		/// Pull a stream using specified URL
		///
		/// @param request_from Source from which RequestPullStream() invoked (Mainly provided when requested by Publisher)
		/// @param vhost_app_name When the URL is pulled, its stream is created in this vhost_name and app_name
		/// @param stream_name When the URL is pulled, its stream is created in this stream_name
		/// @param url URL to pull
		bool RequestPullStream(
			const std::shared_ptr<const ov::Url> &request_from,
			const info::VHostAppName &vhost_app_name, const ov::String &stream_name,
			const ov::String &url)
		{
			return RequestPullStream(request_from, vhost_app_name, stream_name, url, 0);
		}

		/// Pull a stream using Origin map with offset
		///
		/// @param request_from Source from which RequestPullStream() invoked (Mainly provided when requested by Publisher)
		/// @param vhost_app_name When the URL is pulled, its stream is created in this vhost_name and app_name
		/// @param stream_name When the URL is pulled, its stream is created in this stream_name
		/// @param offset Parameters to be used when you want to pull from a certain point (available only when the provider supports that)
		bool RequestPullStream(
			const std::shared_ptr<const ov::Url> &request_from,
			const info::VHostAppName &vhost_app_name, const ov::String &stream_name,
			off_t offset);

		/// Pull a stream using Origin map with offset
		///
		/// @param request_from Source from which RequestPullStream() invoked (Mainly provided when requested by Publisher)
		/// @param vhost_app_name When the URL is pulled, its stream is created in this vhost_name and app_name
		/// @param stream_name When the URL is pulled, its stream is created in this stream_name
		bool RequestPullStream(
			const std::shared_ptr<const ov::Url> &request_from,
			const info::VHostAppName &vhost_app_name, const ov::String &stream_name)
		{
			return RequestPullStream(request_from, vhost_app_name, stream_name, 0);
		}

		/// Find Publisher from PublisehrType
		std::shared_ptr<pub::Publisher> GetPublisherFromType(const PublisherType type);


		//--------------------------------------------------------------------
		// Implementation of ocst::Application::CallbackInterface
		//--------------------------------------------------------------------
		bool OnStreamCreated(const info::Application &app_info, const std::shared_ptr<info::Stream> &info) override;
		bool OnStreamDeleted(const info::Application &app_info, const std::shared_ptr<info::Stream> &info) override;
		bool OnStreamPrepared(const info::Application &app_info, const std::shared_ptr<info::Stream> &info) override;

	protected:
		std::recursive_mutex _module_list_mutex;
		mutable std::recursive_mutex _virtual_host_map_mutex;
	};
}  // namespace ocst
