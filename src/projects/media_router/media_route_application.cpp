//==============================================================================
//
//  MediaRouteApplication
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include <base/info/stream.h>
#include "media_route_application.h"
#include "monitoring/monitoring.h"

#define OV_LOG_TAG "MediaRouter.App"

using namespace common;

std::shared_ptr<MediaRouteApplication> MediaRouteApplication::Create(const info::Application &application_info)
{
	auto media_route_application = std::make_shared<MediaRouteApplication>(application_info);
	if (!media_route_application->Start())
	{
		return nullptr;
	}
	return media_route_application;
}

MediaRouteApplication::MediaRouteApplication(const info::Application &application_info)
	: _application_info(application_info)
{
	logtd("Created media route application. application (%s)", application_info.GetName().CStr());
}

MediaRouteApplication::~MediaRouteApplication()
{
	logtd("Destroyed media router application. application (%s)", _application_info.GetName().CStr());
}

bool MediaRouteApplication::Start()
{
	try
	{
		_kill_flag = false;
		_thread = std::thread(&MediaRouteApplication::MainTask, this);
	}
	catch (const std::system_error &e)
	{
		_kill_flag = true;
		logte("Failed to start media route application thread.");
		return false;
	}

	logtd("started media route application thread. application(%s)", _application_info.GetName().CStr());
	return true;
}

bool MediaRouteApplication::Stop()
{
	_kill_flag = true;
	_indicator.abort();
	if(_thread.joinable())
	{
		_thread.join();
	}

	_connectors.clear();
	_observers.clear();

	return true;
}

// Called when an application is created
bool MediaRouteApplication::RegisterConnectorApp(
	std::shared_ptr<MediaRouteApplicationConnector> app_conn)
{
	if (app_conn == nullptr)
	{
		return false;
	}

	logtd("Register connector. app(%s/%p) type(%d)", _application_info.GetName().CStr(), app_conn.get(), app_conn->GetConnectorType());

	std::lock_guard<std::shared_mutex> lock(_connectors_lock);
	app_conn->SetMediaRouterApplication(GetSharedPtr());
	_connectors.push_back(app_conn);

	return true;
}

// Called when an application is remvoed
bool MediaRouteApplication::UnregisterConnectorApp(
	std::shared_ptr<MediaRouteApplicationConnector> app_conn)
{
	if (app_conn == nullptr)
	{
		return false;
	}
	
	logtd("Unregister connector. app(%s/%p) type(%d)", _application_info.GetName().CStr(), app_conn.get(), app_conn->GetConnectorType());
	
	std::lock_guard<std::shared_mutex> lock(_connectors_lock);

	auto position = std::find(_connectors.begin(), _connectors.end(), app_conn);
	if (position == _connectors.end())
	{
		return true;
	}

	_connectors.erase(position);

	return true;
}

bool MediaRouteApplication::RegisterObserverApp(
	std::shared_ptr<MediaRouteApplicationObserver> app_obsrv)
{
	if (app_obsrv == nullptr)
	{
		return false;
	}

	logtd("Register observer. app(%s/%p) type(%d)", _application_info.GetName().CStr(), app_obsrv.get(), app_obsrv->GetObserverType());

	std::lock_guard<std::shared_mutex> lock(_observers_lock);
	_observers.push_back(app_obsrv);

	return true;
}

bool MediaRouteApplication::UnregisterObserverApp(
	std::shared_ptr<MediaRouteApplicationObserver> app_obsrv)
{
	if (app_obsrv == nullptr)
	{
		return false;
	}

	logtd("Unregister observer. app(%s/%p) type(%d)", _application_info.GetName().CStr(), app_obsrv.get(), app_obsrv->GetObserverType());

	std::lock_guard<std::shared_mutex> lock(_observers_lock);

	auto position = std::find(_observers.begin(), _observers.end(), app_obsrv);
	if (position == _observers.end())
	{
		return true;
	}

	_observers.erase(position);

	return true;
}


// OnCreateStream is called from Provider and Transcoder
bool MediaRouteApplication::OnCreateStream(
	const std::shared_ptr<MediaRouteApplicationConnector> &app_conn,
	const std::shared_ptr<info::Stream> &stream_info)
{
	if (app_conn == nullptr || stream_info == nullptr)
	{
		OV_ASSERT2(false);
		return false;
	}

	// If there is same stream, reuse that
	if (app_conn->GetConnectorType() == MediaRouteApplicationConnector::ConnectorType::Provider)
	{
		std::lock_guard<std::shared_mutex> lock_guard(_streams_lock);

		for (auto it = _streams.begin(); it != _streams.end(); ++it)
		{
			auto istream = it->second;

			if (stream_info->GetName() == istream->GetStream()->GetName())
			{
				// reuse stream
				stream_info->SetId(istream->GetStream()->GetId());
				logtw("Reconnected same stream from provider(%s, %d)", stream_info->GetName().CStr(), stream_info->GetId());

				return true;
			}
		}
	}

	logti("Trying to create a stream: [%s/%s(%u)]", _application_info.GetName().CStr(), stream_info->GetName().CStr(), stream_info->GetId());

	auto new_stream = std::make_shared<MediaRouteStream>(stream_info);
	new_stream->SetConnectorType(app_conn->GetConnectorType());

	{
		std::lock_guard<std::shared_mutex> lock_guard(_streams_lock);
		_streams.insert(std::make_pair(stream_info->GetId(), new_stream));
	}

	// For Monitoring
	mon::Monitoring::GetInstance()->OnStreamCreated(*stream_info);

	{
		std::shared_lock<std::shared_mutex> lock(_observers_lock);
		// Notify all observers that a stream has been created
		for (auto observer : _observers)
		{
			if (
				// Provider -> MediaRoute -> Transcoder
				(app_conn->GetConnectorType() == MediaRouteApplicationConnector::ConnectorType::Provider) &&
				(observer->GetObserverType() == MediaRouteApplicationObserver::ObserverType::Transcoder))
			{
				observer->OnCreateStream(new_stream->GetStream());
			}
			else if (
				// Provider -> MediaRoute -> Relay
				(app_conn->GetConnectorType() == MediaRouteApplicationConnector::ConnectorType::Provider) &&
				(observer->GetObserverType() == MediaRouteApplicationObserver::ObserverType::Relay))
			{
				observer->OnCreateStream(new_stream->GetStream());
			}
			else if (
				// Transcoder -> MediaRoute -> Publisher
				(app_conn->GetConnectorType() == MediaRouteApplicationConnector::ConnectorType::Transcoder) &&
				(observer->GetObserverType() == MediaRouteApplicationObserver::ObserverType::Publisher))
			{
				observer->OnCreateStream(new_stream->GetStream());
			}
			else if (
				// RelayClient -> MediaRoute -> Publisher
				(app_conn->GetConnectorType() == MediaRouteApplicationConnector::ConnectorType::Relay) &&
				(observer->GetObserverType() == MediaRouteApplicationObserver::ObserverType::Publisher))
			{
				observer->OnCreateStream(new_stream->GetStream());
			}
		}
	}

	return true;
}

bool MediaRouteApplication::OnDeleteStream(
	const std::shared_ptr<MediaRouteApplicationConnector> &app_conn,
	const std::shared_ptr<info::Stream> &stream_info)
{
	if (app_conn == nullptr || stream_info == nullptr)
	{
		logte("Invalid arguments: connector: %p, stream: %p", app_conn.get(), stream_info.get());
		return false;
	}

	logti("Trying to delete a stream: [%s/%s(%u)]", _application_info.GetName().CStr(), stream_info->GetName().CStr(), stream_info->GetId());

	// For Monitoring
	mon::Monitoring::GetInstance()->OnStreamDeleted(*stream_info);

	logtd("Deleted connector. type(%d), app(%s) stream(%s/%u)", app_conn->GetConnectorType(), _application_info.GetName().CStr(), stream_info->GetName().CStr(), stream_info->GetId());

	// Notify all observers that stream has been deleted
	{
		std::shared_lock<std::shared_mutex> lock_guard(_observers_lock);
		for (auto it = _observers.begin(); it != _observers.end(); ++it)
		{
			auto observer = *it;

			if (
				(app_conn->GetConnectorType() == MediaRouteApplicationConnector::ConnectorType::Provider) &&
				((observer->GetObserverType() == MediaRouteApplicationObserver::ObserverType::Transcoder) ||
				(observer->GetObserverType() == MediaRouteApplicationObserver::ObserverType::Relay) ||
				(observer->GetObserverType() == MediaRouteApplicationObserver::ObserverType::Orchestrator) ))
			{
				observer->OnDeleteStream(stream_info);
			}
			else if (
				(app_conn->GetConnectorType() == MediaRouteApplicationConnector::ConnectorType::Transcoder) &&
				((observer->GetObserverType() == MediaRouteApplicationObserver::ObserverType::Publisher) ||
				(observer->GetObserverType() == MediaRouteApplicationObserver::ObserverType::Relay) ||
				(observer->GetObserverType() == MediaRouteApplicationObserver::ObserverType::Orchestrator)))
			{
				observer->OnDeleteStream(stream_info);
			}
			else if (
				(app_conn->GetConnectorType() == MediaRouteApplicationConnector::ConnectorType::Relay) &&
				((observer->GetObserverType() == MediaRouteApplicationObserver::ObserverType::Transcoder) ||
				(observer->GetObserverType() == MediaRouteApplicationObserver::ObserverType::Publisher) ||
				(observer->GetObserverType() == MediaRouteApplicationObserver::ObserverType::Orchestrator)))
			{
				observer->OnDeleteStream(stream_info);
			}
		}
	}

	{
		std::lock_guard<std::shared_mutex> lock_guard(_streams_lock);
		_streams.erase(stream_info->GetId());
	}

	return true;
}

// @from RtmpProvider
// @from TranscoderProvider
bool MediaRouteApplication::OnReceiveBuffer(
	const std::shared_ptr<MediaRouteApplicationConnector> &app_conn,
	const std::shared_ptr<info::Stream> &stream_info,
	const std::shared_ptr<MediaPacket> &packet)
{
	if (app_conn == nullptr || stream_info == nullptr)
	{
		return false;
	}

	// 스트림 ID에 해당하는 스트림을 탐색
	std::shared_ptr<MediaRouteStream> stream = nullptr;

	{
		std::shared_lock<std::shared_mutex> lock_guard(_streams_lock);

		auto stream_bucket = _streams.find(stream_info->GetId());
		if (stream_bucket == _streams.end())
		{
			logte("cannot find stream from router. appication(%s), stream(%s)", _application_info.GetName().CStr(), stream_info->GetName().CStr());

			return false;
		}

		stream = stream_bucket->second;
	}

	if (stream == nullptr)
	{
		logte("invalid stream bucket");
		return false;
	}

	bool ret = stream->Push(packet);
	if(ret == true)
	{
		_indicator.push(std::make_shared<BufferIndicator>(stream_info->GetId()));
	}
	
	return ret;
}

void MediaRouteApplication::OnGarbageCollector()
{
	// _indicator.push(std::make_shared<BufferIndicator>(BUFFFER_INDICATOR_UNIQUEID_GC));
}

void MediaRouteApplication::GarbageCollector()
{
#if 0	
	time_t curr_time;
	time(&curr_time);

	std::lock_guard<decltype(_mutex)> lock_guard(_mutex);

	for (auto it = _streams.begin(); it != _streams.end(); ++it)
	{
		auto stream = (*it).second;

		MediaRouteApplicationConnector::ConnectorType connector_type = stream->GetConnectorType();

		if (connector_type == MediaRouteApplicationConnector::ConnectorType::Provider)
		{
			double diff_time = difftime(curr_time, stream->getLastReceivedTime());

			// Streams that do not receive data are automatically deleted after 30 seconds.
			if (diff_time > TIMEOUT_STREAM_ALIVE)
			{
				logti("%s(%u) will delete the stream. diff time(%.0f)",
					  stream->GetStream()->GetName().CStr(),
					  stream->GetStream()->GetId(),
					  diff_time);

				for (auto observer : _observers)
				{
					if (observer->GetObserverType() == MediaRouteApplicationObserver::ObserverType::Transcoder)
					{
						observer->OnDeleteStream(stream->GetStream());
					}
				}

				std::unique_lock<std::mutex> lock(_mutex);
				_streams.erase(stream->GetStream()->GetId());

				// 비효율적임
				it = _streams.begin();
			}
		}
	}
#endif	
}

void MediaRouteApplication::MainTask()
{
	while (!_kill_flag)
	{
		auto indicator = _indicator.pop_unique();
		if (indicator == nullptr)
		{
			// It may be called due to a normal stop signal.
			continue;
		}

		// GC 수행
		if (indicator->_stream_id == BUFFFER_INDICATOR_UNIQUEID_GC)
		{
			GarbageCollector();
			continue;
		}

		std::shared_lock<std::shared_mutex> lock(_streams_lock);
		auto it = _streams.find(indicator->_stream_id);
		auto stream = (it != _streams.end()) ? it->second : nullptr;
		lock.unlock();

		if (stream == nullptr)
		{
			// TODO (Soulk): I changed logte to logtd because this can happen even under normal situation such as stream deleted. Check this.
			logtd("stream is nullptr - strem_id(%u)", indicator->_stream_id);
			continue;
		}

		while(auto media_packet = stream->Pop())
		{
			MediaRouteApplicationConnector::ConnectorType connector_type = stream->GetConnectorType();

			auto stream_info = stream->GetStream();

			// Find Media Track
			auto media_track = stream_info->GetTrack(media_packet->GetTrackId());

			// Observer(Publisher or Transcoder)에게 MediaBuffer를 전달함.
			std::shared_lock<std::shared_mutex> lock(_observers_lock);
			for (const auto &observer : _observers)
			{
				// Provider -> MediaRouter -> Transcoder
				if (
				    (connector_type == MediaRouteApplicationConnector::ConnectorType::Provider) &&
				    (observer->GetObserverType() == MediaRouteApplicationObserver::ObserverType::Transcoder))
				{
					// TODO(soulk): Application Observer의 타입에 따라서 호출하는 함수를 다르게 한다
					auto media_buffer_clone = media_packet->ClonePacket();

					observer->OnSendFrame(stream_info, std::move(media_buffer_clone));
				}
				// Transcoder -> MediaRouter -> Publisher
				// or
				// RelayClient -> MediaRouter -> Publisher
				else if (((connector_type == MediaRouteApplicationConnector::ConnectorType::Transcoder) ||
				          (connector_type == MediaRouteApplicationConnector::ConnectorType::Relay)) &&
				         (observer->GetObserverType() == MediaRouteApplicationObserver::ObserverType::Publisher))
				{
					if (media_packet->GetMediaType() == MediaType::Video)
					{
						// logtd("send video packet to publisher %lld", media_packet->GetPts());
						observer->OnSendVideoFrame(stream_info, media_packet);
					}
					else if (media_packet->GetMediaType() == MediaType::Audio)
					{
						// logtd("send audio packet to publisher %lld", media_packet->GetPts());
						observer->OnSendAudioFrame(stream_info, media_packet);
					}
				}
			}
			lock.unlock();
		}
	}
}
