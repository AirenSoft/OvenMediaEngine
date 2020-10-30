//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#include "vhosts_controller.h"

#include <config/config.h>

#include "../../../api_private.h"
#include "apps/apps_controller.h"

namespace api
{
	namespace v1
	{
		void VHostsController::PrepareHandlers()
		{
			RegisterGet(R"()", &VHostsController::OnGetVhostList);
			RegisterGet(R"(\/(?<vhost_name>[^\/]*))", &VHostsController::OnGetVhost);

			CreateSubController<v1::AppsController>(R"(\/(?<vhost_name>[^\/]*)\/apps)");
		}

		ApiResponse VHostsController::OnGetVhostList(const std::shared_ptr<HttpClient> &client)
		{
			const auto &vhost_list_config = cfg::ConfigManager::GetInstance()->GetServer()->GetVirtualHostList();
			Json::Value response(Json::ValueType::arrayValue);

			for (const auto &vhost : vhost_list_config)
			{
				response.append(vhost.GetName().CStr());
			}

			return response;
		}

		void SetOptionalString(Json::Value &object, const char *key, const ov::String &value)
		{
			if (value.IsEmpty() == false)
			{
				object[key] = value.CStr();
			}
		}

		Json::Value FillVhost(const cfg::VirtualHost &vhost)
		{
			Json::Value response;

			// Field: String name
			response["name"] = vhost.GetName().CStr();

			// Field: [Optional] Host host
			const auto &host_config = vhost.GetHost();
			if (host_config.IsParsed())
			{
				Json::Value &host = response["host"];

				// Field: List<String> names
				Json::Value &names = host["names"];
				for (const auto &name_config : host_config.GetNameList())
				{
					names.append(name_config.GetName().CStr());
				}

				// Field: [Optional] Tls tls
				const auto &tls_config = host_config.GetTls();
				if (tls_config.IsParsed())
				{
					Json::Value &tls = host["tls"];

					// Field: String certPath
					tls["certPath"] = tls_config.GetCertPath().CStr();
					// Field: String keyPath
					tls["keyPath"] = tls_config.GetKeyPath().CStr();
					// Field: [Optional] String chainCertPath
					SetOptionalString(tls, "chainCertPath", tls_config.GetChainCertPath());
				}
			}

			// Field: [Optional] SignedPolicy signedPolicy
			const auto &signed_policy_config = vhost.GetSignedPolicy();
			if (signed_policy_config.IsParsed())
			{
				Json::Value &signed_policy = response["signedPolicy"];

				// Field: String policyQueryKey
				signed_policy["policyQueryKey"] = signed_policy_config.GetPolicyQueryKey().CStr();
				// Field: String signatureQueryKey
				signed_policy["signatureQueryKey"] = signed_policy_config.GetSignatureQueryKey().CStr();
				// Field: String secretKey
				signed_policy["secretKey"] = signed_policy_config.GetSecretKey().CStr();
			}

			// Field: [Optional] SignedToken signedToken
			const auto &signed_token_config = vhost.GetSignedToken();
			if (signed_token_config.IsParsed())
			{
				Json::Value &signed_token = response["signedToken"];

				// Field: String cryptoKey
				signed_token["cryptoKey"] = signed_token_config.GetCryptoKey().CStr();
				// Field: String queryStringKey
				signed_token["queryStringKey"] = signed_token_config.GetQueryStringKey().CStr();
			}

			// Field: [Optional] List<OriginMap> originMaps
			const auto &origins_config = vhost.GetOrigins();
			if (origins_config.IsParsed())
			{
				Json::Value &origin_maps = response["originMaps"];

				for (auto &origin_config : origins_config.GetOriginList())
				{
					Json::Value origin;

					// Field: String location
					origin["location"] = origin_config.GetLocation().CStr();

					// Field: Pass pass
					const auto &pass_config = origin_config.GetPass();
					{
						Json::Value &pass = origin["pass"];

						// Field: String scheme
						pass["scheme"] = pass_config.GetScheme().CStr();

						// Field: List<String> urls
						const auto &url_list_config = pass_config.GetUrlList();
						{
							Json::Value &url_list = pass["urls"];

							for (auto &url : url_list_config)
							{
								url_list.append(url.GetUrl().CStr());
							}
						}
					}

					origin_maps.append(origin);
				}
			}

			return response;
		}

		ApiResponse VHostsController::OnGetVhost(const std::shared_ptr<HttpClient> &client)
		{
			// Get resources from URI
			auto &match_result = client->GetRequest()->GetMatchResult();
			auto vhost_name = match_result.GetNamedGroup("vhost_name");

			const auto &vhost_list_config = cfg::ConfigManager::GetInstance()->GetServer()->GetVirtualHostList();

			for (const auto &vhost : vhost_list_config)
			{
				if (vhost_name == vhost.GetName().CStr())
				{
					return FillVhost(vhost);
				}
			}

			return ov::Error::CreateError(HttpStatusCode::NotFound, "Could not find virtual host: [%.*s]", vhost_name.length(), vhost_name.data());
		}

	}  // namespace v1
}  // namespace api
