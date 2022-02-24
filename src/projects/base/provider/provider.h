//==============================================================================
//
//  Provider Base Class
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/common_types.h>
#include <base/mediarouter/mediarouter_interface.h>
#include <orchestrator/data_structures/data_structure.h>

#include <modules/access_control/access_controller.h>

#include <shared_mutex>

namespace pvd
{
	class Application;
	class Stream;
	class Provider : public ocst::ModuleInterface
	{
	public:
		virtual ProviderType GetProviderType() const = 0;
		virtual ProviderStreamDirection GetProviderStreamDirection() const = 0;
		virtual const char* GetProviderName() const= 0;

		virtual bool Start();
		virtual bool Stop();

		std::shared_ptr<Application> GetApplicationByName(const info::VHostAppName &vhost_app_name);
		std::shared_ptr<Stream> GetStreamByName(const info::VHostAppName &vhost_app_name, ov::String stream_name);

		std::shared_ptr<Application> GetApplicationById(info::application_id_t app_id);
		std::shared_ptr<Stream> GetStreamById(info::application_id_t app_id, uint32_t stream_id);

		std::tuple<AccessController::VerificationResult, std::shared_ptr<const SignedPolicy>> VerifyBySignedPolicy(const std::shared_ptr<const ov::Url> &request_url, const std::shared_ptr<ov::SocketAddress> &client_address);

		std::tuple<AccessController::VerificationResult, std::shared_ptr<const AdmissionWebhooks>> SendCloseAdmissionWebhooks(const std::shared_ptr<const ov::Url> &request_url, const std::shared_ptr<ov::SocketAddress> &client_address);
		std::tuple<AccessController::VerificationResult, std::shared_ptr<const AdmissionWebhooks>> VerifyByAdmissionWebhooks(const std::shared_ptr<const ov::Url> &request_url, const std::shared_ptr<ov::SocketAddress> &client_address);

	protected:
		Provider(const cfg::Server &server_config, const std::shared_ptr<MediaRouteInterface> &router);
		virtual ~Provider();

		const cfg::Server &GetServerConfig() const;

		// For child class
		virtual std::shared_ptr<Application> OnCreateProviderApplication(const info::Application &app_info) = 0;
		virtual bool OnDeleteProviderApplication(const std::shared_ptr<pvd::Application> &application) = 0;

		//--------------------------------------------------------------------
		// Implementation of ModuleInterface
		//--------------------------------------------------------------------
		bool OnCreateApplication(const info::Application &app_info) override;
		bool OnDeleteApplication(const info::Application &app_info) override;
		
	private:	
		const cfg::Server _server_config;
		
		std::map<info::application_id_t, std::shared_ptr<Application>> _applications;
		std::shared_mutex  _application_map_mutex;
		std::shared_ptr<MediaRouteInterface> _router;
		std::shared_ptr<AccessController> _access_controller = nullptr;
	};

}  // namespace pvd
