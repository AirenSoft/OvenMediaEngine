//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#include "segment_publisher.h"

#include <modules/access_control/signed_token/signed_token.h>
#include <monitoring/monitoring.h>
#include <orchestrator/orchestrator.h>
#include <publishers/segment/segment_stream/segment_stream.h>

#include "publisher_private.h"

SegmentPublisher::SegmentPublisher(const cfg::Server &server_config, const std::shared_ptr<MediaRouteInterface> &router)
	: Publisher(server_config, router)
{
}

SegmentPublisher::~SegmentPublisher()
{
	logtd("Publisher has been destroyed");
}

bool SegmentPublisher::Start(const cfg::cmn::SingularPort &port_config, const cfg::cmn::SingularPort &tls_port_config, const std::shared_ptr<SegmentStreamServer> &stream_server, int worker_count)
{
	auto server_config = GetServerConfig();
	auto ip = server_config.GetIp();

	auto port = port_config.GetPort();
	auto tls_port = tls_port_config.GetPort();
	bool has_port = (port != 0);
	bool has_tls_port = (tls_port != 0);

	ov::SocketAddress address(ip, port);
	ov::SocketAddress tls_address(ip, tls_port);

	// Register as observer
	stream_server->AddObserver(SegmentStreamObserver::GetSharedPtr());

	// Apply CORS settings
	// TODO(Dimiden): Move this code to the HttpInterceptor
	//stream_server->SetCrossDomain(cross_domains);

	if (stream_server->Start(has_port ? &address : nullptr, has_tls_port ? &tls_address : nullptr,
							 DEFAULT_SEGMENT_WORKER_THREAD_COUNT, worker_count) == false)
	{
		logte("An error occurred while start %s Publisher", GetPublisherName());
		return false;
	}

	_stream_server = stream_server;

	logti("%s is listening on %s%s%s%s...",
		  GetPublisherName(),
		  has_port ? address.ToString().CStr() : "",
		  (has_port && has_tls_port) ? ", " : "",
		  has_tls_port ? "TLS: " : "",
		  has_tls_port ? tls_address.ToString().CStr() : "");

	return Publisher::Start();
}

bool SegmentPublisher::Stop()
{
	_run_thread = false;
	if (_worker_thread.joinable())
	{
		_worker_thread.join();
	}

	_stream_server->RemoveObserver(SegmentStreamObserver::GetSharedPtr());
	_stream_server->Stop();
	return Publisher::Stop();
}

bool SegmentPublisher::OnPlayListRequest(const std::shared_ptr<http::svr::HttpConnection> &client,
										 const SegmentStreamRequestInfo &request_info,
										 ov::String &play_list)
{
	auto request = client->GetRequest();
	auto uri = request->GetUri();
	auto parsed_url = ov::Url::Parse(uri);

	if (parsed_url == nullptr)
	{
		logte("Could not parse the url: %s", uri.CStr());
		client->GetResponse()->SetStatusCode(http::StatusCode::BadRequest);
		// Returns true when the observer search can be ended.
		return true;
	}

	auto &vhost_app_name = request_info.vhost_app_name;
	auto &stream_name = request_info.stream_name;

	std::shared_ptr<PlaylistRequestInfo> playlist_request_info;
	if (HandleSignedX(vhost_app_name, stream_name, client, parsed_url, playlist_request_info) == false)
	{
		client->GetResponse()->SetStatusCode(http::StatusCode::Forbidden);
		return true;
	}

	auto stream = GetStreamAs<SegmentStream>(vhost_app_name, stream_name);
	if (stream == nullptr)
	{
		stream = std::dynamic_pointer_cast<SegmentStream>(PullStream(parsed_url, vhost_app_name, request_info.host_name, stream_name));
		if (stream == nullptr)
		{
			client->GetResponse()->SetStatusCode(http::StatusCode::NotFound);
			return true;
		}
		else
		{
			// Connection Request log
			// 2019-11-06 09:46:45.390 , RTSP.SS ,REQUEST,INFO,,,Live,rtsp://50.1.111.154:10915/1135/1/,220.103.225.254_44757_1573001205_389304_128855562
			stat_log(STAT_LOG_HLS_EDGE_REQUEST, "%s,%s,%s,%s,,,%s,%s,%s",
					 ov::Clock::Now().CStr(),
					 "HLS.SS",
					 "REQUEST",
					 "INFO",
					 vhost_app_name.CStr(),
					 stream->GetMediaSource().CStr(),
					 playlist_request_info != nullptr ? playlist_request_info->GetSessionId().CStr() : client->GetRequest()->GetRemote()->GetRemoteAddress()->GetIpAddress().CStr());

			logti("URL %s is requested", stream->GetMediaSource().CStr());
		}

		if (stream->WaitUntilStart(3000) == false)
		{
			logtw("(%s/%s) stream has not started.", vhost_app_name.CStr(), stream_name.CStr());
			// Returns true when the observer search can be ended.
			return true;
		}
	}

	request->SetExtra(std::static_pointer_cast<pub::Stream>(stream));

	if (stream->GetPlayList(play_list) == false)
	{
		logtw("Could not get a playlist for %s [%p, %s/%s, %s]", GetPublisherName(), stream.get(), vhost_app_name.CStr(), stream_name.CStr(), request_info.file_name.CStr());
		client->GetResponse()->SetStatusCode(http::StatusCode::Accepted);
		// Returns true when the observer search can be ended.
		return true;
	}

	client->GetResponse()->SetStatusCode(http::StatusCode::OK);
	return true;
}

bool SegmentPublisher::OnSegmentRequest(const std::shared_ptr<http::svr::HttpConnection> &client,
										const SegmentStreamRequestInfo &request_info,
										std::shared_ptr<const SegmentItem> &segment)
{
	auto &vhost_app_name = request_info.vhost_app_name;
	auto &stream_name = request_info.stream_name;
	auto &file_name = request_info.file_name;

	auto stream = GetStreamAs<SegmentStream>(vhost_app_name, stream_name);

	if (stream != nullptr)
	{
		segment = stream->GetSegmentData(file_name);

		if (segment == nullptr)
		{
			logtw("Could not find a segment for %s [%s/%s, %s]", GetPublisherName(), vhost_app_name.CStr(), stream_name.CStr(), file_name.CStr());
			return false;
		}
		else if (segment->data == nullptr)
		{
			logtw("Could not obtain segment data from %s for [%p, %s/%s, %s]", GetPublisherName(), segment.get(), vhost_app_name.CStr(), stream_name.CStr(), file_name.CStr());
			return false;
		}
	}
	else
	{
		logtw("Could not find a stream for %s [%s/%s, %s]", GetPublisherName(), vhost_app_name.CStr(), stream_name.CStr(), file_name.CStr());
		return false;
	}

	// To manage sessions
	logti("Segment requested (%s/%s/%s) from %s : Segment number : %u Duration : %u",
		  vhost_app_name.CStr(), stream_name.CStr(), file_name.CStr(),
		  client->GetRequest()->GetRemote()->GetRemoteAddress()->ToString().CStr(),
		  segment->sequence_number, segment->duration_in_ms / 1000);

	client->GetRequest()->SetExtra(std::static_pointer_cast<pub::Stream>(stream));

	// The first sequence number 0 means init_video and init_audio in MPEG-DASH.
	// These are excluded because they confuse statistical calculations.
	if(GetPublisherType() != PublisherType::LlDash &&  segment->sequence_number != 0)
	{
		auto segment_request_info = SegmentRequestInfo(GetPublisherType(),
													*std::static_pointer_cast<info::Stream>(stream),
													client->GetRequest()->GetRemote()->GetRemoteAddress()->GetIpAddress(),				
													segment->sequence_number,
													segment->type,		
													segment->duration_in_ms / 1000);
		UpdateSegmentRequestInfo(segment_request_info);
	}

	return true;
}

bool SegmentPublisher::StartSessionTableManager()
{
	_run_thread = true;
	_worker_thread = std::thread(&SegmentPublisher::RequestTableUpdateThread, this);
	pthread_setname_np(_worker_thread.native_handle(), "SegPubReq");

	return true;
}

void SegmentPublisher::RequestTableUpdateThread()
{
	auto _last_logging_time = std::chrono::system_clock::now();

	while (_run_thread)
	{
		// This time, only log for HLS - later it will be changed
		if (GetPublisherType() == PublisherType::Hls)
		{
			// Concurrent user log
			std::chrono::system_clock::time_point current;
			uint32_t duration;

			current = std::chrono::system_clock::now();
			duration = std::chrono::duration_cast<std::chrono::seconds>(current - _last_logging_time).count();
			if (duration > 60)
			{
				// 2018-12-24 23:06:25.035,RTSP.SS,CONN_COUNT,INFO,,,[Live users], [Playback users]
				std::shared_ptr<info::Application> rtsp_live_app_info;
				std::shared_ptr<mon::ApplicationMetrics> rtsp_live_app_metrics;
				std::shared_ptr<info::Application> rtsp_play_app_info;
				std::shared_ptr<mon::ApplicationMetrics> rtsp_play_app_metrics;

				rtsp_live_app_metrics = nullptr;
				rtsp_play_app_metrics = nullptr;

				// This log only for the "default" host and the "rtsp_live"/"rtsp_playback" applications
				rtsp_live_app_info = std::static_pointer_cast<info::Application>(GetApplicationByName(ocst::Orchestrator::GetInstance()->ResolveApplicationName("default", "rtsp_live")));
				if (rtsp_live_app_info != nullptr)
				{
					rtsp_live_app_metrics = ApplicationMetrics(*rtsp_live_app_info);
				}
				rtsp_play_app_info = std::static_pointer_cast<info::Application>(GetApplicationByName(ocst::Orchestrator::GetInstance()->ResolveApplicationName("default", "rtsp_playback")));
				if (rtsp_play_app_info != nullptr)
				{
					rtsp_play_app_metrics = ApplicationMetrics(*rtsp_play_app_info);
				}

				stat_log(STAT_LOG_HLS_EDGE_VIEWERS, "%s,%s,%s,%s,,,%u,%u",
						 ov::Clock::Now().CStr(),
						 "HLS.SS",
						 "CONN_COUNT",
						 "INFO",
						 rtsp_live_app_metrics != nullptr ? rtsp_live_app_metrics->GetTotalConnections() : 0,
						 rtsp_play_app_metrics != nullptr ? rtsp_play_app_metrics->GetTotalConnections() : 0);

				_last_logging_time = std::chrono::system_clock::now();
			}
		}

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

					auto playlist_request_info = GetSessionRequestInfoBySegmentRequestInfo(*request_info);
					stat_log(STAT_LOG_HLS_EDGE_SESSION, "%s,%s,%s,%s,,,%s,%s,%s",
							 ov::Clock::Now().CStr(),
							 "HLS.SS",
							 "SESSION",
							 "INFO",
							 "deleteClientSession",
							 request_info->GetStreamInfo().GetName().CStr(),
							 playlist_request_info != nullptr ? playlist_request_info->GetSessionId().CStr() : request_info->GetIpAddress().CStr());

					std::shared_ptr<info::Application> rtsp_live_app_info;
					std::shared_ptr<mon::ApplicationMetrics> rtsp_live_app_metrics;
					std::shared_ptr<info::Application> rtsp_play_app_info;
					std::shared_ptr<mon::ApplicationMetrics> rtsp_play_app_metrics;

					rtsp_live_app_metrics = nullptr;
					rtsp_play_app_metrics = nullptr;

					// This log only for the "default" host and the "rtsp_live"/"rtsp_playback" applications
					rtsp_live_app_info = std::static_pointer_cast<info::Application>(GetApplicationByName(ocst::Orchestrator::GetInstance()->ResolveApplicationName("default", "rtsp_live")));
					if (rtsp_live_app_info != nullptr)
					{
						rtsp_live_app_metrics = ApplicationMetrics(*rtsp_live_app_info);
					}
					rtsp_play_app_info = std::static_pointer_cast<info::Application>(GetApplicationByName(ocst::Orchestrator::GetInstance()->ResolveApplicationName("default", "rtsp_playback")));
					if (rtsp_play_app_info != nullptr)
					{
						rtsp_play_app_metrics = ApplicationMetrics(*rtsp_play_app_info);
					}

					stat_log(STAT_LOG_HLS_EDGE_SESSION, "%s,%s,%s,%s,,,%s:%d,%s:%d,%s,%s",
							 ov::Clock::Now().CStr(),
							 "HLS.SS",
							 "SESSION",
							 "INFO",
							 "Live",
							 rtsp_live_app_metrics != nullptr ? rtsp_live_app_metrics->GetTotalConnections() : 0,
							 "Playback",
							 rtsp_play_app_metrics != nullptr ? rtsp_play_app_metrics->GetTotalConnections() : 0,
							 request_info->GetStreamInfo().GetName().CStr(),
							 playlist_request_info != nullptr ? playlist_request_info->GetSessionId().CStr() : request_info->GetIpAddress().CStr());
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

const std::shared_ptr<PlaylistRequestInfo> SegmentPublisher::GetSessionRequestInfoBySegmentRequestInfo(const SegmentRequestInfo &info)
{
	for (const auto &item : _playlist_request_table)
	{
		auto &request_info = item.second;
		if (request_info->GetPublisherType() == info.GetPublisherType() &&
			request_info->GetIpAddress() == info.GetIpAddress() &&
			request_info->GetStreamName() == info.GetStreamInfo().GetName() &&
			request_info->GetAppName() == info.GetStreamInfo().GetApplicationInfo().GetName())
		{
			return request_info;
		}
	}

	return nullptr;
}

void SegmentPublisher::UpdatePlaylistRequestInfo(const std::shared_ptr<PlaylistRequestInfo> &info)
{
	std::unique_lock<std::recursive_mutex> table_lock(_playlist_request_table_lock);
	// Change info to new one
	//TODO(Getroot): In the future, by comparing the existing data with the creation time, it can be identified as normal.
	if (_playlist_request_table.count(info->GetSessionId().CStr()) == 0)
	{
		logti("Authorize session : %s/%s - %s - %s", info->GetAppName().CStr(), info->GetStreamName().CStr(),
			  info->GetSessionId().CStr(), info->GetIpAddress().CStr());
	}

	_playlist_request_table[info->GetSessionId().CStr()] = info;
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

void SegmentPublisher::UpdateSegmentRequestInfo(SegmentRequestInfo &info)
{
	logtd("Update segment request info - Segment number : %u Duration : %u Type : %s", info.GetSequenceNumber(), info.GetDuration(), info.GetDataType()==SegmentDataType::Video?"Video":"Audio");

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

			// If the segment is divided into audio and video, statistics must be calculated using only one.
			if(item->GetDataType() != info.GetDataType())
			{
				return;
			}

			if (item->IsNextRequest(info))
			{
				auto count = item->GetCount();
				info.SetCount(count++);

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

	_segment_request_table.insert(std::pair<std::string, std::shared_ptr<SegmentRequestInfo>>(info.GetIpAddress().CStr(), std::make_shared<SegmentRequestInfo>(info)));

	table_lock.unlock();

	// It is a new viewer!
	if (new_session)
	{
		// New Session!!!
		auto stream_metrics = StreamMetrics(info.GetStreamInfo());
		if (stream_metrics != nullptr)
		{
			stream_metrics->OnSessionConnected(info.GetPublisherType());

			auto playlist_request_info = GetSessionRequestInfoBySegmentRequestInfo(info);
			stat_log(STAT_LOG_HLS_EDGE_SESSION, "%s,%s,%s,%s,,,%s,%s,%s",
					 ov::Clock::Now().CStr(),
					 "HLS.SS",
					 "SESSION",
					 "INFO",
					 "createClientSession",
					 info.GetStreamInfo().GetName().CStr(),
					 playlist_request_info != nullptr ? playlist_request_info->GetSessionId().CStr() : info.GetIpAddress().CStr());

			std::shared_ptr<info::Application> rtsp_live_app_info;
			std::shared_ptr<mon::ApplicationMetrics> rtsp_live_app_metrics;
			std::shared_ptr<info::Application> rtsp_play_app_info;
			std::shared_ptr<mon::ApplicationMetrics> rtsp_play_app_metrics;

			rtsp_live_app_metrics = nullptr;
			rtsp_play_app_metrics = nullptr;

			// This log only for the "default" host and the "rtsp_live"/"rtsp_playback" applications
			rtsp_live_app_info = std::static_pointer_cast<info::Application>(GetApplicationByName(ocst::Orchestrator::GetInstance()->ResolveApplicationName("default", "rtsp_live")));
			if (rtsp_live_app_info != nullptr)
			{
				rtsp_live_app_metrics = ApplicationMetrics(*rtsp_live_app_info);
			}
			rtsp_play_app_info = std::static_pointer_cast<info::Application>(GetApplicationByName(ocst::Orchestrator::GetInstance()->ResolveApplicationName("default", "rtsp_playback")));
			if (rtsp_play_app_info != nullptr)
			{
				rtsp_play_app_metrics = ApplicationMetrics(*rtsp_play_app_info);
			}

			stat_log(STAT_LOG_HLS_EDGE_SESSION, "%s,%s,%s,%s,,,%s:%d,%s:%d,%s,%s",
					 ov::Clock::Now().CStr(),
					 "HLS.SS",
					 "SESSION",
					 "INFO",
					 "Live",
					 rtsp_live_app_metrics != nullptr ? rtsp_live_app_metrics->GetTotalConnections() : 0,
					 "Playback",
					 rtsp_play_app_metrics != nullptr ? rtsp_play_app_metrics->GetTotalConnections() : 0,
					 info.GetStreamInfo().GetName().CStr(),
					 playlist_request_info != nullptr ? playlist_request_info->GetSessionId().CStr() : info.GetIpAddress().CStr());
		}
	}
}

bool SegmentPublisher::HandleSignedX(const info::VHostAppName &vhost_app_name, const ov::String &stream_name,
									 const std::shared_ptr<http::svr::HttpConnection> &client, const std::shared_ptr<const ov::Url> &request_url,
									 std::shared_ptr<PlaylistRequestInfo> &request_info)
{
	auto request = client->GetRequest();
	auto remote_address = request->GetRemote()->GetRemoteAddress();
	if (remote_address == nullptr)
	{
		OV_ASSERT2(false);
		logtc("Invalid remote address found");
		return false;
	}

	auto requested_url = ov::Url::Parse(request_url->ToUrlString(true));
	// PORT can be omitted if port is rtmp default port, but SignedPolicy requires this information.
	if(requested_url->Port() == 0)
	{
		requested_url->SetPort(request->GetRemote()->GetLocalAddress()->Port());
	}

	auto session_id = remote_address->GetIpAddress();
	request_info = std::make_shared<PlaylistRequestInfo>(GetPublisherType(),
															vhost_app_name, stream_name,
															remote_address->GetIpAddress(),
															session_id);

	// SingedPolicy is first
	auto [signed_policy_result, signed_policy] = Publisher::VerifyBySignedPolicy(requested_url, remote_address);
	if (signed_policy_result == AccessController::VerificationResult::Pass)
	{
		UpdatePlaylistRequestInfo(request_info);
		return true;
	}
	else if (signed_policy_result == AccessController::VerificationResult::Error)
	{
		return false;
	}
	else if (signed_policy_result == AccessController::VerificationResult::Fail)
	{
		if (signed_policy->GetErrCode() == SignedPolicy::ErrCode::URL_EXPIRED)
		{
			// Because this is chunked streaming publisher,
			// players will continue to request playlists after token expiration while playing.
			// Therefore, once authorized session must be maintained.
			if (IsAuthorizedSession(*request_info))
			{
				// Update the authorized session info
				UpdatePlaylistRequestInfo(request_info);
				return true;
			}
		}

		logtw("%s", signed_policy->GetErrMessage().CStr());
		return false;
	}
	else if (signed_policy_result == AccessController::VerificationResult::Off)
	{
		// SingedToken
		auto [signed_token_result, signed_token] = Publisher::VerifyBySignedToken(request_url, remote_address);

		if (signed_token_result == AccessController::VerificationResult::Pass)
		{
			UpdatePlaylistRequestInfo(request_info);
			return true;
		}
		else if (signed_token_result == AccessController::VerificationResult::Off)
		{
			return true;
		}
		else if (signed_token_result == AccessController::VerificationResult::Error)
		{
			return false;
		}
		else if (signed_token_result == AccessController::VerificationResult::Fail)
		{
			if (signed_token->GetErrCode() == SignedToken::ErrCode::TOKEN_EXPIRED)
			{
				// Because this is chunked streaming publisher,
				// players will continue to request playlists after token expiration while playing.
				// Therefore, once authorized session must be maintained.
				if (IsAuthorizedSession(*request_info))
				{
					// Update the authorized session info
					UpdatePlaylistRequestInfo(request_info);
					return true;
				}
			}

			logtw("%s", signed_token->GetErrMessage().CStr());
			return false;
		}
	}

	return true;
}
