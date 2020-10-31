//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#include "vhost.h"

#include "converter_private.h"

namespace api
{
	namespace conv
	{
		static void SetNames(Json::Value &parent_object, const char *key, const cfg::Names &config, Optional optional)
		{
			CONVERTER_RETURN_IF(false);

			for (const auto &name_config : config.GetNameList())
			{
				object.append(name_config.GetName().CStr());
			}
		}

		static void SetTls(Json::Value &parent_object, const char *key, const cfg::Tls &config, Optional optional)
		{
			CONVERTER_RETURN_IF(config.IsParsed() == false);

			SetString(object, "certPath", config.GetCertPath(), Optional::False);
			SetString(object, "keyPath", config.GetKeyPath(), Optional::False);
			SetString(object, "chainCertPath", config.GetChainCertPath(), Optional::True);
		}

		static void SetHost(Json::Value &parent_object, const char *key, const cfg::Host &config, Optional optional)
		{
			CONVERTER_RETURN_IF(config.IsParsed() == false);

			SetNames(object, "names", config.GetNames(), Optional::False);
			SetTls(object, "tls", config.GetTls(), Optional::True);
		}

		static void SetSignedPolicy(Json::Value &parent_object, const char *key, const cfg::SignedPolicy &config, Optional optional)
		{
			CONVERTER_RETURN_IF(config.IsParsed() == false);

			SetString(object, "policyQueryKey", config.GetPolicyQueryKey(), Optional::False);
			SetString(object, "signatureQueryKey", config.GetSignatureQueryKey(), Optional::False);
			SetString(object, "secretKey", config.GetSecretKey(), Optional::False);
		}

		static void SetSignedToken(Json::Value &parent_object, const char *key, const cfg::SignedToken &config, Optional optional)
		{
			CONVERTER_RETURN_IF(config.IsParsed() == false);

			SetString(object, "cryptoKey", config.GetCryptoKey(), Optional::False);
			SetString(object, "queryStringKey", config.GetQueryStringKey(), Optional::False);
		}

		static void SetUrls(Json::Value &parent_object, const char *key, const cfg::Urls &config, Optional optional)
		{
			CONVERTER_RETURN_IF(config.IsParsed() == false);

			for (auto &url_config : config.GetUrlList())
			{
				object.append(url_config.GetUrl().CStr());
			}
		}

		static void SetPass(Json::Value &parent_object, const char *key, const cfg::Pass &config, Optional optional)
		{
			CONVERTER_RETURN_IF(config.IsParsed() == false);

			SetString(object, "scheme", config.GetScheme(), Optional::False);
			SetUrls(object, "urls", config.GetUrls(), Optional::False);
		}

		static void SetOriginMaps(Json::Value &parent_object, const char *key, const cfg::Origins &config, Optional optional)
		{
			CONVERTER_RETURN_IF(config.IsParsed() == false);

			for (auto &origin_config : config.GetOriginList())
			{
				Json::Value origin;

				SetString(origin, "location", origin_config.GetLocation(), Optional::False);
				SetPass(origin, "pass", origin_config.GetPass(), Optional::False);

				object.append(origin);
			}
		}

		Json::Value ConvertFromVHost(const std::shared_ptr<const ocst::VirtualHost> &vhost)
		{
			Json::Value response;

			const auto &host_info = vhost->host_info;

			SetString(response, "name", host_info.GetName().CStr(), Optional::False);
			SetHost(response, "host", host_info.GetHost(), Optional::True);
			SetSignedPolicy(response, "signedPolicy", host_info.GetSignedPolicy(), Optional::True);
			SetSignedToken(response, "signedToken", host_info.GetSignedToken(), Optional::True);
			SetOriginMaps(response, "originMaps", host_info.GetOrigins(), Optional::True);

			return response;
		}

	}  // namespace conv
};	   // namespace api