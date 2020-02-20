//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#include "hls_publisher.h"
#include "hls_application.h"
#include "hls_private.h"
#include "hls_stream_server.h"

#include <config/config_manager.h>
#include <modules/signed_url/signed_url.h>
#include <orchestrator/orchestrator.h>

std::shared_ptr<HlsPublisher> HlsPublisher::Create(std::map<int, std::shared_ptr<HttpServer>> &http_server_manager,
												   const cfg::Server &server_config,
												   const info::Host &host_info,
												   const std::shared_ptr<MediaRouteInterface> &router)
{
	return SegmentPublisher::Create<HlsPublisher>(http_server_manager, server_config, host_info, router);
}

HlsPublisher::HlsPublisher(PrivateToken token,
						   const cfg::Server &server_config,
						   const info::Host &host_info,
						   const std::shared_ptr<MediaRouteInterface> &router)
	: SegmentPublisher(server_config, host_info, router)
{
}

bool HlsPublisher::HandleSignedUrl(const std::shared_ptr<HttpClient> &client, const std::shared_ptr<const ov::Url> &request_url)
{
	auto orchestrator = Orchestrator::GetInstance();
	auto &server_config = GetServerConfig();
	auto &vhost_list = server_config.GetVirtualHostList();
	auto vhost_name = orchestrator->GetVhostNameFromDomain(request_url->Domain());

	if (vhost_name.IsEmpty())
	{
		logtw("Could not resolve the domain: %s", request_url->Domain().CStr());
		return false;
	}

	for (auto &vhost_item : vhost_list)
	{
		if (vhost_item.GetName() == vhost_name)
		{
			// Handle Signed URL if needed
			auto &signed_url_config = vhost_item.GetSignedUrl();

			if (signed_url_config.IsParsed())
			{
				auto crypto_key = signed_url_config.GetCryptoKey();
				auto query_string_key = signed_url_config.GetQueryStringKey();
				auto &query_map = request_url->QueryMap();

				auto item = query_map.find(query_string_key);
				if (item == query_map.end())
				{
					logtw("Could not find key %s in query string in URL: %s", query_string_key.CStr(), request_url->Source().CStr());
					return false;
				}

				auto signed_url = SignedUrl::Load(SignedUrlType::Type0, crypto_key, item->second);
				auto remote_address = client->GetRemote()->GetRemoteAddress();

				if (remote_address == nullptr)
				{
					OV_ASSERT2(false);
					logtc("Invalid remote address found");
					return false;
				}

				if (signed_url == nullptr)
				{
					logte("Could not obtain decrypted information of the signed url: %s, key: %s, value: %s", request_url->Source().CStr(), query_string_key.CStr(), item->second.CStr());
					return false;
				}

				auto rtsp_item = query_map.find("rtspURI");
				if(rtsp_item == query_map.end())
				{			
					logte("Could not find rtspURI in query string");
					return false;
				}

				auto url_to_compare = request_url->ToUrlString(false);
				url_to_compare.AppendFormat("?rtspURI=%s", ov::Url::Encode(rtsp_item->second).CStr());

				if (
					signed_url->IsTokenExpired() ||
					signed_url->IsStreamExpired() ||
					(signed_url->IsAllowedClient(*remote_address) == false) ||
					(signed_url->GetUrl().UpperCaseString() != url_to_compare.UpperCaseString()))
				{
					std::vector<ov::String> messages;
					auto now = signed_url->GetNowMS();

					if (signed_url->IsTokenExpired())
					{
						messages.push_back(ov::String::FormatString(
							"Token is expired: %lld (Now: %lld)",
							signed_url->GetTokenExpiredTime(),
							now));
					}

					if (signed_url->IsStreamExpired())
					{
						messages.push_back(ov::String::FormatString(
							"Stream is expired: %lld (Now: %lld)",
							signed_url->GetStreamExpiredTime(),
							now));
					}

					if (signed_url->IsAllowedClient(*remote_address) == false)
					{
						messages.push_back(ov::String::FormatString(
							"Not allowed: %s (Expected: %s)",
							remote_address->ToString().CStr(),
							signed_url->GetClientIP().CStr()));
					}

					if (signed_url->GetUrl() != url_to_compare)
					{
						messages.push_back(ov::String::FormatString(
							"Invalid URL: %s (Expected: %s)",
							url_to_compare.CStr(),
							signed_url->GetUrl().CStr()));
					}

					logtw("Failed to authenticate client %s\nReason:\n    - %s",
						  client->GetRemote()->ToString().CStr(),
						  ov::String::Join(messages, "\n    - ").CStr());

					return false;
				}
			}
		}
	}

	return true;
}

bool HlsPublisher::Start(std::map<int, std::shared_ptr<HttpServer>> &http_server_manager)
{
	auto &server_config = GetServerConfig();
	auto &host_info = GetHostInfo();

	//auto &name = host_info.GetName();

	auto &ip = server_config.GetIp();
	auto port = server_config.GetBind().GetPublishers().GetDashPort();

	ov::SocketAddress address(ip, port);

	// Register as observer
	auto stream_server = std::make_shared<HlsStreamServer>();
	stream_server->AddObserver(SegmentStreamObserver::GetSharedPtr());

	// Apply CORS settings
	// TODO(Dimiden): The Cross Domain configure must be at VHost Level.
	//stream_server->SetCrossDomain(cross_domains);

	// Start the HLS Server
	if (stream_server->Start(address, http_server_manager, DEFAULT_SEGMENT_WORKER_THREAD_COUNT,
							 host_info.GetCertificate(), host_info.GetChainCertificate()) == false)
	{
		logte("An error occurred while start %s Publisher", GetPublisherName());
		return false;
	}

	_stream_server = stream_server;

	return Publisher::Start();
}

bool HlsPublisher::OnPlayListRequest(const std::shared_ptr<HttpClient> &client,
									 const ov::String &app_name, const ov::String &stream_name,
									 const ov::String &file_name,
									 ov::String &play_list)
{
	auto &request = client->GetRequest();
	auto uri = request->GetUri();
	auto parsed_url = ov::Url::Parse(uri.CStr(), true);

	if (parsed_url == nullptr)
	{
		logte("Could not parse the url: %s", uri.CStr());
		return false;
	}

	// These names are used for testing purposes
	// TODO(dimiden): Need to delete this code after testing
	if (app_name.HasSuffix("_insecure") == false)
	{
		if (HandleSignedUrl(client, parsed_url) == false)
		{
			return false;
		}
	}

	auto stream = GetStreamAs<SegmentStream>(app_name, stream_name);

	if (stream == nullptr)
	{
		// These names are used for testing purposes
		// TODO(dimiden): Need to delete this code after testing
		if (
			app_name.HasSuffix("#rtsp_live") || app_name.HasSuffix("#rtsp_playback") ||
			app_name.HasSuffix("#rtsp_live_insecure") || app_name.HasSuffix("#rtsp_playback_insecure"))
		{
			auto orchestrator = Orchestrator::GetInstance();

			auto &query_map = parsed_url->QueryMap();

			auto rtsp_uri_item = query_map.find("rtspURI");

			if (rtsp_uri_item == query_map.end())
			{
				logte("There is no rtspURI parameter in the query string: %s", uri.CStr());
				logtd("Query map:");
				for (auto &query : query_map)
				{
					logtd("    %s = %s", query.first.CStr(), query.second.CStr());
				}
				return false;
			}

			auto rtsp_uri = rtsp_uri_item->second;

			if (orchestrator->RequestPullStream(app_name, stream_name, rtsp_uri) == false)
			{
				logte("Could not request pull stream for URL: %s", rtsp_uri.CStr());
				return false;
			}

			logti("URL %s is requested", rtsp_uri.CStr());
		}
	}

	if ((stream == nullptr) || (stream->GetPlayList(play_list) == false))
	{
		logtw("Could not get a playlist for %s [%p, %s/%s, %s]", GetPublisherName(), stream.get(), app_name.CStr(), stream_name.CStr(), file_name.CStr());
		return false;
	}

	return true;
}

std::shared_ptr<pub::Application> HlsPublisher::OnCreatePublisherApplication(const info::Application &application_info)
{
	if (application_info.CheckCodecAvailability({"h264"}, {"aac"}) == false)
	{
		logtw("There is no suitable encoding setting for %s (Encoding setting must contains h264 and aac)", GetPublisherName());
	}

	return HlsApplication::Create(application_info);
}
