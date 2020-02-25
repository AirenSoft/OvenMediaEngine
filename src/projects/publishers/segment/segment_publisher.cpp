//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#include "segment_publisher.h"
#include "publisher_private.h"

#include <modules/signed_url/signed_url.h>
#include <monitoring/monitoring.h>
#include <orchestrator/orchestrator.h>
#include <publishers/segment/segment_stream/segment_stream.h>

SegmentPublisher::SegmentPublisher(const cfg::Server &server_config, const info::Host &host_info, const std::shared_ptr<MediaRouteInterface> &router)
	: Publisher(server_config, host_info, router)
{
}

SegmentPublisher::~SegmentPublisher()
{
	_run_thread = false;
	logtd("Publisher has been destroyed");
}

bool SegmentPublisher::GetMonitoringCollectionData(std::vector<std::shared_ptr<pub::MonitoringCollectionData>> &collections)
{
	return (_stream_server != nullptr) ? _stream_server->GetMonitoringCollectionData(collections) : false;
}

bool SegmentPublisher::OnPlayListRequest(const std::shared_ptr<HttpClient> &client,
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
		if (HandleSignedUrl(app_name, stream_name, client, parsed_url) == false)
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

			stream = GetStreamAs<SegmentStream>(app_name, stream_name);
		}
	}

	if ((stream == nullptr) || (stream->GetPlayList(play_list) == false))
	{
		logtw("Could not get a playlist for %s [%p, %s/%s, %s]", GetPublisherName(), stream.get(), app_name.CStr(), stream_name.CStr(), file_name.CStr());
		return false;
	}

	return true;
}

bool SegmentPublisher::OnSegmentRequest(const std::shared_ptr<HttpClient> &client,
										const ov::String &app_name, const ov::String &stream_name,
										const ov::String &file_name,
										std::shared_ptr<SegmentData> &segment)
{
	auto stream = GetStreamAs<SegmentStream>(app_name, stream_name);

	if (stream != nullptr)
	{
		segment = stream->GetSegmentData(file_name);

		if (segment == nullptr)
		{
			logtw("Could not find a segment for %s [%s/%s, %s]", GetPublisherName(), app_name.CStr(), stream_name.CStr(), file_name.CStr());
			return false;
		}
		else if (segment->data == nullptr)
		{
			logtw("Could not obtain segment data from %s for [%p, %s/%s, %s]", GetPublisherName(), segment.get(), app_name.CStr(), stream_name.CStr(), file_name.CStr());
			return false;
		}
	}
	else
	{
		logtw("Could not find a stream for %s [%s/%s, %s]", GetPublisherName(), app_name.CStr(), stream_name.CStr(), file_name.CStr());
		return false;
	}

	// To manage sessions
	UpdateSegmentRequestInfo(SegmentRequestInfo(GetPublisherType(),
												*std::static_pointer_cast<info::Stream>(stream),
												client->GetRemote()->GetRemoteAddress()->GetIpAddress(),
												segment->sequence_number,
												segment->duration));

	return true;
}

bool SegmentPublisher::StartSessionTableManager()
{
	_run_thread = true;
	_worker_thread = std::thread(&SegmentPublisher::RequestTableUpdateThread, this);
	_worker_thread.detach();

	return true;
}

void SegmentPublisher::RequestTableUpdateThread()
{
	while (_run_thread)
	{
		// Remove a quite old session request info
		// Only HLS/DASH/CMAF publishers have some items.
		std::unique_lock<std::recursive_mutex> segment_table_lock(_segment_request_table_lock);

		for (auto item = _segment_request_table.begin(); item != _segment_request_table.end();)
		{
			auto request_info = item->second;
			if (request_info->IsExpiredRequest())
			{
				// remove and report
				auto stream_metrics = StreamMetrics(request_info->GetStreamInfo());
				if (stream_metrics != nullptr)
				{
					stream_metrics->OnSessionDisconnected(request_info->GetPublisherType());
				}

				item = _segment_request_table.erase(item);
			}
			else
			{
				++item;
			}
		}

		segment_table_lock.unlock();

		std::unique_lock<std::recursive_mutex> playlist_table_lock(_playlist_request_table_lock);

		for (auto item = _playlist_request_table.begin(); item != _playlist_request_table.end();)
		{
			auto request_info = item->second;
			if (request_info->IsTooOld())
			{
				// remove and report
				logti("Remove the permission of the authorized session : %s/%s - %s - %s", 
													request_info->GetAppName().CStr(), request_info->GetStreamName().CStr(),
													request_info->GetSessionId().CStr(), request_info->GetIpAddress().CStr());
				item = _playlist_request_table.erase(item);
			}
			else
			{
				++item;
			}
		}

		playlist_table_lock.unlock();

		sleep(3);
	}
}

void SegmentPublisher::UpdatePlaylistRequestInfo(const PlaylistRequestInfo &info)
{
	std::unique_lock<std::recursive_mutex> table_lock(_playlist_request_table_lock);
	// Change info to new one
	//TODO(Getroot): In the future, by comparing the existing data with the creation time, it can be identified as normal.
	if(_playlist_request_table.count(info.GetSessionId().CStr()) == 0)
	{
		logti("Authorize session : %s/%s - %s - %s", info.GetAppName().CStr(), info.GetStreamName().CStr(),
													info.GetSessionId().CStr(), info.GetIpAddress().CStr());
	}

	_playlist_request_table[info.GetSessionId().CStr()] = std::make_shared<PlaylistRequestInfo>(info);
}

bool SegmentPublisher::IsAuthorizedSession(const PlaylistRequestInfo &info)
{
	auto select_count = _playlist_request_table.count(info.GetSessionId().CStr());
	if (select_count > 0)
	{
		auto item = _playlist_request_table.at(info.GetSessionId().CStr());
		if (item->IsRequestFromSameUser(info))
		{
			return true;
		}
	}
	return false;
}

void SegmentPublisher::UpdateSegmentRequestInfo(const SegmentRequestInfo &info)
{
	bool new_session = true;
	std::unique_lock<std::recursive_mutex> table_lock(_segment_request_table_lock);

	auto select_count = _segment_request_table.count(info.GetIpAddress().CStr());
	if (select_count > 0)
	{
		// select * where IP=info.ip from _session_table
		auto it = _segment_request_table.equal_range(info.GetIpAddress().CStr());
		for (auto itr = it.first; itr != it.second;)
		{
			auto item = itr->second;

			if (item->IsNextRequest(info))
			{
				itr = _segment_request_table.erase(itr);
				new_session = false;
				break;
			}
			else
			{
				++itr;
			}
		}
	}

	// It is a new viewer!
	if (new_session)
	{
		auto stream_metrics = StreamMetrics(info.GetStreamInfo());
		if (stream_metrics != nullptr)
		{
			stream_metrics->OnSessionConnected(info.GetPublisherType());
		}
	}

	_segment_request_table.insert(std::pair<std::string, std::shared_ptr<SegmentRequestInfo>>(info.GetIpAddress().CStr(), std::make_shared<SegmentRequestInfo>(info)));
}

bool SegmentPublisher::HandleSignedUrl(const ov::String &app_name, const ov::String &stream_name, const std::shared_ptr<HttpClient> &client, const std::shared_ptr<const ov::Url> &request_url)
{
	auto orchestrator = Orchestrator::GetInstance();
	auto &server_config = GetServerConfig();
	auto vhost_name = orchestrator->GetVhostNameFromDomain(request_url->Domain());

	if (vhost_name.IsEmpty())
	{
		logtw("Could not resolve the domain: %s", request_url->Domain().CStr());
		return false;
	}
	
	// TODO(Dimiden) : Modify blow codes
	// GetVirtualHostByName is deprecated so blow codes are insane, later it will be modified. 
	auto vhost_list = server_config.GetVirtualHostList();
	for (const auto &vhost_item : vhost_list)
	{
		if (vhost_item.GetName() != vhost_name)
		{
			continue;
		}
		// Found

		// Handle Signed URL if needed
		auto &signed_url_config = vhost_item.GetSignedUrl();
		if (!signed_url_config.IsParsed())
		{
			// The vhost doesn't use the signed url feature.
			return true;
		}

		auto remote_address = client->GetRemote()->GetRemoteAddress();
		if (remote_address == nullptr)
		{
			OV_ASSERT2(false);
			logtc("Invalid remote address found");
			return false;
		}

		auto &query_map = request_url->QueryMap();

		// Load config (crypto key, query string key)
		auto crypto_key = signed_url_config.GetCryptoKey();
		auto query_string_key = signed_url_config.GetQueryStringKey();

		// Find a encoded string in query string
		auto item = query_map.find(query_string_key);
		if (item == query_map.end())
		{
			logtw("Could not find key %s in query string in URL: %s", query_string_key.CStr(), request_url->Source().CStr());
			return false;
		}

		// Find a rtspURI in query string
		auto rtsp_item = query_map.find("rtspURI");
		if (rtsp_item == query_map.end())
		{
			logte("Could not find rtspURI in query string");
			return false;
		}

		// Decoding and parsing
		auto signed_url = SignedUrl::Load(SignedUrlType::Type0, crypto_key, item->second);
		if (signed_url == nullptr)
		{
			logte("Could not obtain decrypted information of the signed url: %s, key: %s, value: %s", request_url->Source().CStr(), query_string_key.CStr(), item->second.CStr());
			return false;
		}

		auto url_to_compare = request_url->ToUrlString(false);
		url_to_compare.AppendFormat("?rtspURI=%s", ov::Url::Encode(rtsp_item->second).CStr());

		std::vector<ov::String> messages;
		bool result = true;
		auto now = signed_url->GetNowMS();
		PlaylistRequestInfo request_info = PlaylistRequestInfo(GetPublisherType(),
															   app_name, stream_name,
															   remote_address->GetIpAddress(),
															   signed_url->GetSessionID());

		if (signed_url->IsTokenExpired())
		{
			// Even if the token has expired, it can still be passed if the session ID had been approved.
			if (!IsAuthorizedSession(request_info))
			{
				messages.push_back(ov::String::FormatString("Token is expired: %lld (Now: %lld)", signed_url->GetTokenExpiredTime(), now));
				result = false;
			}
		}

		if (signed_url->IsStreamExpired())
		{
			messages.push_back(ov::String::FormatString("Stream is expired: %lld (Now: %lld)", signed_url->GetStreamExpiredTime(), now));
			result = false;
		}

		if (signed_url->IsAllowedClient(*remote_address) == false)
		{
			messages.push_back(ov::String::FormatString("Not allowed: %s (Expected: %s)",
														remote_address->ToString().CStr(),
														signed_url->GetClientIP().CStr()));
			result = false;
		}

		if (signed_url->GetUrl().UpperCaseString() != url_to_compare.UpperCaseString())
		{
			messages.push_back(ov::String::FormatString("Invalid URL: %s (Expected: %s)",
														url_to_compare.CStr(),
														signed_url->GetUrl().CStr()));
			result = false;
		}

		if (result == false)
		{
			logtw("Failed to authenticate client %s\nReason:\n    - %s",
				  client->GetRemote()->ToString().CStr(),
				  ov::String::Join(messages, "\n    - ").CStr());

			return false;
		}

		// Update the authorized session info
		UpdatePlaylistRequestInfo(request_info);

		return true;
	}

	return false;
}
