//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2022 AirenSoft. All rights reserved.
//
//==============================================================================
#include "cors_manager.h"

#include "../http_private.h"

static constexpr char CORS_HTTP_PREFIX[] = "http://";
static constexpr auto CORS_HTTP_PREFIX_LENGTH = OV_COUNTOF(CORS_HTTP_PREFIX) - 1;
static constexpr char CORS_HTTPS_PREFIX[] = "https://";
static constexpr auto CORS_HTTPS_PREFIX_LENGTH = OV_COUNTOF(CORS_HTTPS_PREFIX) - 1;

namespace http
{
	void CorsManager::SetCrossDomains(const info::VHostAppName &vhost_app_name, const std::vector<ov::String> &url_list)
	{
		std::lock_guard lock_guard(_cors_mutex);

		auto &cors_policy = _cors_policy_map[vhost_app_name];
		auto &cors_regex_list = _cors_item_list_map[vhost_app_name];
		ov::String cors_rtmp;

		auto cors_domains_for_rtmp = std::vector<ov::String>();

		cors_regex_list.clear();
		cors_rtmp = "";

		if (url_list.size() == 0)
		{
			cors_policy = CorsPolicy::Empty;
			return;
		}

		cors_policy = CorsPolicy::Origin;

		for (auto url : url_list)
		{
			if (url == "*")
			{
				cors_regex_list.clear();
				cors_regex_list.emplace_back(false, ov::Regex::CompiledRegex(ov::Regex::WildCardRegex("*")));
				cors_policy = CorsPolicy::All;

				if (url_list.size() > 1)
				{
					// Ignore other items if "*" is specified
					logtw("Invalid CORS settings found for %s: '*' cannot be used like other items. Other items are ignored.", vhost_app_name.CStr());
				}

				break;
			}
			else if (url == "null")
			{
				if (url_list.size() > 1)
				{
					logtw("Invalid CORS settings found for %s: '*' cannot be used like other items. 'null' item is ignored.", vhost_app_name.CStr());
				}
				else
				{
					cors_policy = CorsPolicy::Null;
				}

				continue;
			}

			bool has_protocol = url.HasPrefix(CORS_HTTP_PREFIX) || url.HasPrefix(CORS_HTTPS_PREFIX);

			cors_regex_list.emplace_back(has_protocol, ov::Regex::CompiledRegex(ov::Regex::WildCardRegex(url)));

			cors_domains_for_rtmp.push_back(url);
		}

		// Create crossdomain.xml for RTMP - OME does not support RTMP playback, so it is not used.
		std::ostringstream cross_domain_xml;

		cross_domain_xml
			<< R"(<?xml version="1.0"?>)" << std::endl
			<< R"(<!DOCTYPE cross-domain-policy SYSTEM "http://www.adobe.com/xml/dtds/cross-domain-policy.dtd">)" << std::endl
			<< R"(<cross-domain-policy>)" << std::endl;

		if (cors_policy == CorsPolicy::All)
		{
			cross_domain_xml
				<< R"(	<site-control permitted-cross-domain-policies="all" />)" << std::endl
				<< R"(	<allow-access-from domain="*" secure="false" />)" << std::endl
				<< R"(	<allow-http-request-headers-from domain="*" headers="*" secure="false"/>)" << std::endl;
		}
		else
		{
			for (auto &cors_url : cors_domains_for_rtmp)
			{
				cross_domain_xml
					<< R"(	<allow-access-from domain=")" << cors_url.CStr() << R"(" />)" << std::endl;
			}
		}

		cross_domain_xml
			<< R"(</cross-domain-policy>)";

		cors_rtmp = cross_domain_xml.str().c_str();

		if (_cors_rtmp.IsEmpty())
		{
			_cors_rtmp = cors_rtmp;
		}
		else
		{
			if (cors_rtmp != _cors_rtmp)
			{
				logtw("Different CORS settings found for RTMP: crossdomain.xml must be located / and cannot be declared per app");
				logtw("This CORS settings will be used\n%s", _cors_rtmp.CStr());
			}
		}
	}

	bool CorsManager::SetupRtmpCorsXml(const std::shared_ptr<http::svr::HttpResponse> &response) const
	{
		std::lock_guard lock_guard(_cors_mutex);

		if (_cors_rtmp.IsEmpty() == false)
		{
			response->SetHeader("Content-Type", "text/x-cross-domain-policy");
			response->AppendString(_cors_rtmp);
		}

		return true;
	}

	bool CorsManager::SetupHttpCorsHeader(const info::VHostAppName &vhost_app_name, const std::shared_ptr<const http::svr::HttpRequest> &request, const std::shared_ptr<http::svr::HttpResponse> &response) const
	{
		ov::String origin_header = request->GetHeader("ORIGIN");
		ov::String cors_header = "";

		{
			std::lock_guard lock_guard(_cors_mutex);

			auto cors_policy_iterator = _cors_policy_map.find(vhost_app_name);
			auto cors_regex_list_iterator = _cors_item_list_map.find(vhost_app_name);

			if (
				(cors_policy_iterator == _cors_policy_map.end()) ||
				(cors_regex_list_iterator == _cors_item_list_map.end()))
			{
				// This happens in the following situations:
				//
				// 1) Request to an application that doesn't exist
				// 2) Request while the application is being created
				return false;
			}

			const auto &cors_policy = cors_policy_iterator->second;

			switch (cors_policy)
			{
				case CorsPolicy::Empty:
					// Nothing to do
					return true;

				case CorsPolicy::All:
					cors_header = "*";
					break;

				case CorsPolicy::Null:
					cors_header = "null";
					break;

				case CorsPolicy::Origin: {
					const auto &cors_item_list = cors_regex_list_iterator->second;
					auto origin_header_without_protocol = origin_header;

					if (origin_header.HasPrefix(CORS_HTTP_PREFIX))
					{
						origin_header_without_protocol = origin_header.Substring(CORS_HTTP_PREFIX_LENGTH);
					}
					else if (origin_header.HasPrefix(CORS_HTTPS_PREFIX))
					{
						origin_header_without_protocol = origin_header.Substring(CORS_HTTPS_PREFIX_LENGTH);
					}

					auto item = std::find_if(
						cors_item_list.begin(), cors_item_list.end(),
						[&origin_header, &origin_header_without_protocol](auto &cors_item) -> bool {
							return cors_item.regex.Matches(cors_item.has_protocol ? origin_header : origin_header_without_protocol).IsMatched();
						});

					if (item == cors_item_list.end())
					{
						// Could not find the domain
						return false;
					}

					cors_header = origin_header;
				}
			}
		}

		response->SetHeader("Access-Control-Allow-Origin", cors_header);
		response->SetHeader("Vary", "Origin");

		response->SetHeader("Access-Control-Allow-Credentials", "true");
		response->SetHeader("Access-Control-Allow-Methods", "GET");
		response->SetHeader("Access-Control-Allow-Headers", "*");

		return true;
	}
}  // namespace http
