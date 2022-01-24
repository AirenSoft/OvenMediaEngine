//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2022 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/info/vhost_app_name.h>
#include <base/ovlibrary/ovlibrary.h>

#include "../server/http_server.h"

namespace http
{
	class CorsManager
	{
	public:
		// url_list can contains such values:
		//   - *: Adds "Access-Control-Allow-Origin: *" for the request
		//        If "*" and <domain> are passed at the same time, the <domain> value is ignored.
		//   - null: Adds "Access-Control-Allow-Origin: null" for the request
		//        If "null" and <domain> are passed at the same time, the "null" value is ignored.
		//   - <domain>: Adds "Access-Control-Allow-Origin: <domain>" for the request
		//
		// Empty url_list means 'Do not set any CORS header'
		//
		// NOTE - SetCrossDomains() isn't thread-safe.
		void SetCrossDomains(const info::VHostAppName &vhost_app_name, const std::vector<ov::String> &url_list);

		bool SetupRtmpCorsXml(const std::shared_ptr<http::svr::HttpResponse> &response) const;
		bool SetupHttpCorsHeader(const info::VHostAppName &vhost_app_name, const std::shared_ptr<const http::svr::HttpRequest> &request, const std::shared_ptr<http::svr::HttpResponse> &response) const;

	protected:
		// https://fetch.spec.whatwg.org/#http-access-control-allow-origin
		// `Access-Control-Allow-Origin`
		// Indicates whether the response can be shared, via returning the literal value of the `Origin` request header
		// (which can be `null`) or `*` in a response.
		//
		// For example:
		// Access-Control-Allow-Origin: *
		// Access-Control-Allow-Origin: null
		// Access-Control-Allow-Origin: <origin>
		enum class CorsPolicy
		{
			// Do not add any CORS header
			Empty,
			// *
			All,
			// null
			Null,
			// Specific domain in _cors_regex_list_map
			Origin
		};

		struct CorsItem
		{
			CorsItem(bool has_protocol, const ov::Regex &regex)
				: has_protocol(has_protocol),
				  regex(regex)
			{
			}

			bool has_protocol;
			ov::Regex regex;
		};

	protected:
		mutable std::mutex _cors_mutex;

		std::unordered_map<info::VHostAppName, CorsPolicy> _cors_policy_map;

		// CORS for HTTP
		// key: VHostAppName, value: regex
		std::unordered_map<info::VHostAppName, std::vector<CorsItem>> _cors_item_list_map;

		// CORS for RTMP
		//
		// NOTE - The RTMP CORS setting follows the first declared <CrossDomains> setting,
		//        because crossdomain.xml must be located / and cannot be declared per app.
		ov::String _cors_rtmp;
	};
}  // namespace http
