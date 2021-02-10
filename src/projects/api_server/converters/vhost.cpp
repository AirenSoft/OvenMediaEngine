//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================
#include "vhost.h"

#include "common.h"

namespace api
{
	namespace conv
	{
		static void SetNames(Json::Value &parent_object, const char *key, const cfg::cmn::Names &config, Optional optional)
		{
			CONVERTER_RETURN_IF(false, Json::arrayValue);

			for (const auto &name_config : config.GetNameList())
			{
				object.append(name_config.CStr());
			}
		}

		static void SetTls(Json::Value &parent_object, const char *key, const cfg::cmn::Tls &config, Optional optional)
		{
			CONVERTER_RETURN_IF(config.IsParsed() == false, Json::objectValue);

			SetString(object, "certPath", config.GetCertPath(), Optional::False);
			SetString(object, "keyPath", config.GetKeyPath(), Optional::False);
			SetString(object, "chainCertPath", config.GetChainCertPath(), Optional::True);
		}

		static void SetHost(Json::Value &parent_object, const char *key, const cfg::cmn::Host &config, Optional optional)
		{
			CONVERTER_RETURN_IF(config.IsParsed() == false, Json::objectValue);

			SetNames(object, "names", config.GetNames(), Optional::False);
			SetTls(object, "tls", config.GetTls(), Optional::True);
		}

		static void SetSignedPolicy(Json::Value &parent_object, const char *key, const cfg::vhost::sig::SignedPolicy &config, Optional optional)
		{
			CONVERTER_RETURN_IF(config.IsParsed() == false, Json::objectValue);

			SetString(object, "policyQueryKey", config.GetPolicyQueryKeyName(), Optional::False);
			SetString(object, "signatureQueryKey", config.GetSignatureQueryKeyName(), Optional::False);
			SetString(object, "secretKey", config.GetSecretKey(), Optional::False);
		}

		static void SetSignedToken(Json::Value &parent_object, const char *key, const cfg::vhost::sig::SignedToken &config, Optional optional)
		{
			CONVERTER_RETURN_IF(config.IsParsed() == false, Json::objectValue);

			SetString(object, "cryptoKey", config.GetCryptoKey(), Optional::False);
			SetString(object, "queryStringKey", config.GetQueryStringKey(), Optional::False);
		}

		static void SetUrls(Json::Value &parent_object, const char *key, const cfg::cmn::Urls &config, Optional optional)
		{
			CONVERTER_RETURN_IF(config.IsParsed() == false, Json::arrayValue);

			for (auto &url_config : config.GetUrlList())
			{
				object.append(url_config.CStr());
			}
		}

		static void SetPass(Json::Value &parent_object, const char *key, const cfg::vhost::orgn::Pass &config, Optional optional)
		{
			CONVERTER_RETURN_IF(config.IsParsed() == false, Json::objectValue);

			SetString(object, "scheme", config.GetScheme(), Optional::False);
			SetUrls(object, "urls", config.GetUrls(), Optional::False);
		}

		static void SetOriginMaps(Json::Value &parent_object, const char *key, const cfg::vhost::orgn::Origins &config, Optional optional)
		{
			CONVERTER_RETURN_IF(config.IsParsed() == false, Json::arrayValue);

			for (auto &origin_config : config.GetOriginList())
			{
				Json::Value origin;

				SetString(origin, "location", origin_config.GetLocation(), Optional::False);
				SetPass(origin, "pass", origin_config.GetPass(), Optional::False);

				object.append(origin);
			}
		}

		Json::Value JsonFromVHost(const std::shared_ptr<const mon::HostMetrics> &vhost)
		{
			Json::Value response(Json::ValueType::objectValue);
			
			SetString(response, "name", vhost->GetName().CStr(), Optional::False);
			SetHost(response, "host", vhost->GetHost(), Optional::True);
			SetSignedPolicy(response, "signedPolicy", vhost->GetSignedPolicy(), Optional::True);
			SetSignedToken(response, "signedToken", vhost->GetSignedToken(), Optional::True);
			SetOriginMaps(response, "originMaps", vhost->GetOrigins(), Optional::True);

			return std::move(response);
		}

	}  // namespace conv
};	   // namespace api
