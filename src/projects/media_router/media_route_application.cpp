//==============================================================================
//
//  MediaRouteApplication
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include <base/info/stream_info.h>
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
	_thread.join();

	return true;
}

// 어플리케이션의 스트림이 생성됨
bool MediaRouteApplication::RegisterConnectorApp(
	std::shared_ptr<MediaRouteApplicationConnector> app_conn)
{
	std::unique_lock<std::mutex> lock(_mutex);

	if (app_conn == nullptr)
	{
		return false;
	}

	logtd("Register application connector. application(%s/%p) connector_type(%d)", _application_info.GetName().CStr(), app_conn.get(), app_conn->GetConnectorType());

	app_conn->SetMediaRouterApplication(GetSharedPtr());

	_connectors.push_back(app_conn);

	return true;
}

// 어플리케이션의 스트림이 생성됨
bool MediaRouteApplication::UnregisterConnectorApp(
	std::shared_ptr<MediaRouteApplicationConnector> app_conn)
{
	if (app_conn == nullptr)
	{
		return false;
	}

	logtd("Unregister application connector. application(%s/%p) connector_type(%d)", _application_info.GetName().CStr(), app_conn.get(), app_conn->GetConnectorType());

	// 삭제
	auto position = std::find(_connectors.begin(), _connectors.end(), app_conn);
	if (position == _connectors.end())
	{
		return true;
	}

	std::unique_lock<std::mutex> lock(_mutex);
	_connectors.erase(position);
	lock.unlock();

	return true;
}

bool MediaRouteApplication::RegisterObserverApp(
	std::shared_ptr<MediaRouteApplicationObserver> app_obsrv)
{
	if (app_obsrv == nullptr)
	{
		return false;
	}

	logtd("Register application observer. application(%s/%p) observer_type(%d)", _application_info.GetName().CStr(), app_obsrv.get(), app_obsrv->GetObserverType());

	std::unique_lock<std::mutex> lock(_mutex);
	_observers.push_back(app_obsrv);
	lock.unlock();

	return true;
}

bool MediaRouteApplication::UnregisterObserverApp(
	std::shared_ptr<MediaRouteApplicationObserver> app_obsrv)
{
	if (app_obsrv == nullptr)
	{
		return false;
	}

	logtd("Unregister application observer. application(%s/%p) observer_type(%d)", _application_info.GetName().CStr(), app_obsrv.get(), app_obsrv->GetObserverType());

	auto position = std::find(_observers.begin(), _observers.end(), app_obsrv);
	if (position == _observers.end())
	{
		return true;
	}

	_observers.erase(position);

	return true;
}


// OnCreateStream 함수는 Provider, Trasncoder 타입의 Connector에서 호출된다
bool MediaRouteApplication::OnCreateStream(
	const std::shared_ptr<MediaRouteApplicationConnector> &app_conn,
	const std::shared_ptr<info::StreamInfo> &stream_info)
{
	if (app_conn == nullptr || stream_info == nullptr)
	{
		OV_ASSERT2(false);
		return false;
	}

	// 동일한 스트림명 Reconnect 되는 경우 처리함.
	// 기존에 사용하던 Stream의 ID를 재사용한다
	if (app_conn->GetConnectorType() == MediaRouteApplicationConnector::ConnectorType::Provider)
	{
		std::lock_guard<decltype(_mutex)> lock_guard(_mutex);

		for (auto it = _streams.begin(); it != _streams.end(); ++it)
		{
			auto istream = it->second;

			if (stream_info->GetName() == istream->GetStreamInfo()->GetName())
			{
				// 기존에 사용하던 ID를 재사용
				stream_info->SetId(istream->GetStreamInfo()->GetId());
				logtw("Reconnected same stream from provider(%s, %d)", stream_info->GetName().CStr(), stream_info->GetId());

				return true;
			}
		}
	}

	logtd("Created stream from connector. connector_type(%d), application(%s) stream(%s/%u)", app_conn->GetConnectorType(), _application_info.GetName().CStr(), stream_info->GetName().CStr(), stream_info->GetId());

	auto new_stream_info = std::make_shared<info::StreamInfo>(*stream_info);
	auto new_stream = std::make_shared<MediaRouteStream>(new_stream_info);

	{
		std::lock_guard<std::mutex> lock_guard(_mutex);

		new_stream->SetConnectorType(app_conn->GetConnectorType());

		_streams.insert(std::make_pair(new_stream_info->GetId(), new_stream));
	}

	// For Monitoring
	mon::Monitoring::GetInstance()->OnStreamCreated(*stream_info);

	// 옵저버에 스트림 생성을 알림
	for (auto observer : _observers)
	{
		if (
			// Provider -> MediaRoute -> Transcoder
			(app_conn->GetConnectorType() == MediaRouteApplicationConnector::ConnectorType::Provider) &&
			(observer->GetObserverType() == MediaRouteApplicationObserver::ObserverType::Transcoder))
		{
			observer->OnCreateStream(new_stream->GetStreamInfo());
		}
		else if (
			// Provider -> MediaRoute -> Relay
			(app_conn->GetConnectorType() == MediaRouteApplicationConnector::ConnectorType::Provider) &&
			(observer->GetObserverType() == MediaRouteApplicationObserver::ObserverType::Relay))
		{
			observer->OnCreateStream(new_stream->GetStreamInfo());
		}
		else if (
			// Transcoder -> MediaRoute -> Publisher
			(app_conn->GetConnectorType() == MediaRouteApplicationConnector::ConnectorType::Transcoder) &&
			(observer->GetObserverType() == MediaRouteApplicationObserver::ObserverType::Publisher))
		{
			observer->OnCreateStream(new_stream->GetStreamInfo());
		}
		else if (
			// RelayClient -> MediaRoute -> Publisher
			(app_conn->GetConnectorType() == MediaRouteApplicationConnector::ConnectorType::Relay) &&
			(observer->GetObserverType() == MediaRouteApplicationObserver::ObserverType::Publisher))
		{
			observer->OnCreateStream(new_stream->GetStreamInfo());
		}
	}

	return true;
}

bool MediaRouteApplication::OnDeleteStream(
	const std::shared_ptr<MediaRouteApplicationConnector> &app_conn,
	const std::shared_ptr<info::StreamInfo> &stream_info)
{
	if (app_conn == nullptr || stream_info == nullptr)
	{
		logte("Invalid arguments: connector: %p, stream_info: %p", app_conn.get(), stream_info.get());
		return false;
	}

	// For Monitoring
	mon::Monitoring::GetInstance()->OnStreamDeleted(*stream_info);

	logtd("Deleted stream from connector. connector_type(%d), application(%s) stream(%s/%u)", app_conn->GetConnectorType(), _application_info.GetName().CStr(), stream_info->GetName().CStr(), stream_info->GetId());
	auto new_stream_info = std::make_shared<info::StreamInfo>(*stream_info);

	// 옵저버에 스트림 삭제를 알림
	for (auto it = _observers.begin(); it != _observers.end(); ++it)
	{
		auto observer = *it;

		if (
			(app_conn->GetConnectorType() == MediaRouteApplicationConnector::ConnectorType::Provider) &&
			((observer->GetObserverType() == MediaRouteApplicationObserver::ObserverType::Transcoder) ||
			 (observer->GetObserverType() == MediaRouteApplicationObserver::ObserverType::Relay) ||
			 (observer->GetObserverType() == MediaRouteApplicationObserver::ObserverType::Orchestrator) ))
		{
			observer->OnDeleteStream(new_stream_info);
		}
		else if (
			(app_conn->GetConnectorType() == MediaRouteApplicationConnector::ConnectorType::Transcoder) &&
			((observer->GetObserverType() == MediaRouteApplicationObserver::ObserverType::Publisher) ||
			 (observer->GetObserverType() == MediaRouteApplicationObserver::ObserverType::Relay) ||
			 (observer->GetObserverType() == MediaRouteApplicationObserver::ObserverType::Orchestrator)))
		{
			observer->OnDeleteStream(new_stream_info);
		}
		else if (
			(app_conn->GetConnectorType() == MediaRouteApplicationConnector::ConnectorType::Relay) &&
			((observer->GetObserverType() == MediaRouteApplicationObserver::ObserverType::Transcoder) ||
			 (observer->GetObserverType() == MediaRouteApplicationObserver::ObserverType::Publisher) ||
			 (observer->GetObserverType() == MediaRouteApplicationObserver::ObserverType::Orchestrator)))
		{
			observer->OnDeleteStream(new_stream_info);
		}
	}

	{
		std::lock_guard<std::mutex> lock_guard(_mutex);
		_streams.erase(new_stream_info->GetId());
	}

	return true;
}

// 프로바이더에서 인코딩된 데이터가 들어옴
// @from RtmpProvider
// @from TranscoderProvider
bool MediaRouteApplication::OnReceiveBuffer(
	const std::shared_ptr<MediaRouteApplicationConnector> &app_conn,
	const std::shared_ptr<info::StreamInfo> &stream_info,
	const std::shared_ptr<MediaPacket> &packet)
{
	if (app_conn == nullptr || stream_info == nullptr)
	{
		return false;
	}

	// 스트림 ID에 해당하는 스트림을 탐색
	std::shared_ptr<MediaRouteStream> stream = nullptr;

	{
		std::lock_guard<decltype(_mutex)> lock_guard(_mutex);

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

	bool ret = stream->Push(std::move(packet));

	// TODO(SOULK) : Connector(Provider, Transcoder)에서 수신된 데이터에 대한 정보를 바로 처리하기 위해 버퍼의 Indicator 정보를
	// MainTask에 전달한다. 패킷이 수신되어 처리(재분배)되는 속도가 0.001초 이하의 초저지연으로 동작하나, 효율적인 구조는
	// 아닌것으로 판단되므로, 향후에 개선이 필요하다
	_indicator.push(std::make_shared<BufferIndicator>(stream_info->GetId()));

	return ret;
}

void MediaRouteApplication::OnGarbageCollector()
{
	// _indicator.push(std::make_shared<BufferIndicator>(BUFFFER_INDICATOR_UNIQUEID_GC));
}

// 스트림 객제중에 데이터가 수신되지 않은 스트림은 N초후에 자동 삭제를 진행함.
void MediaRouteApplication::GarbageCollector()
{
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

			// TODO(soulk) 삭제 Treshold 값은 설정서 읽어오도록 수정해야함.
			// Observer 에게 삭제 메세지를 전달하는 구조도 비효율적임. 레이스컨디션 발생 가능성 있음.

			// Streams that do not receive data are automatically deleted after 30 seconds.
			if (diff_time > TIMEOUT_STREAM_ALIVE)
			{
				logti("%s(%u) will delete the stream. diff time(%.0f)",
					  stream->GetStreamInfo()->GetName().CStr(),
					  stream->GetStreamInfo()->GetId(),
					  diff_time);

				for (auto observer : _observers)
				{
					if (observer->GetObserverType() == MediaRouteApplicationObserver::ObserverType::Transcoder)
					{
						observer->OnDeleteStream(stream->GetStreamInfo());
					}
				}

				std::unique_lock<std::mutex> lock(_mutex);
				_streams.erase(stream->GetStreamInfo()->GetId());

				// 비효율적임
				it = _streams.begin();
			}
		}
	}
}

// Stream 객체 에에 있는 패킷을 Application Observer에 전달한다
// TODO: 이 구조에서 Segment Fault 문제가 발생함
// TODO: 패킷을 지연없이 전달하기위해 Application당 스레드를 생성하였음.
void MediaRouteApplication::MainTask()
{
	while (!_kill_flag)
	{
		auto indicator = _indicator.pop_unique();
		if (indicator == nullptr)
		{
			logte("invalid indicator");
			continue;
		}

		// GC 수행
		if (indicator->_stream_id == BUFFFER_INDICATOR_UNIQUEID_GC)
		{
			GarbageCollector();
			continue;
		}

		// logtd("indicator : %u", indicator->_stream_id);

		// TODO: 동기화 처리가 필요 하지만 자주 호출 부분이라 성능 적으로 확인 필요
		std::unique_lock<std::mutex> lock(_mutex);
		auto it = _streams.find(indicator->_stream_id);

		// stream : std::shared_ptr<MediaRouteStream>
		auto stream = (it != _streams.end()) ? it->second : nullptr;
		lock.unlock();

		if (stream == nullptr)
		{
			logte("stream is nullptr - strem_id(%u)", indicator->_stream_id);
			continue;
		}

		// cur_buf : std::shared_ptr<MediaPacket>
		auto media_packet = stream->Pop();
		if (media_packet)
		{
			MediaRouteApplicationConnector::ConnectorType connector_type = stream->GetConnectorType();

			auto stream_info = stream->GetStreamInfo();

			// Find Media Track
			auto media_track = stream_info->GetTrack(media_packet->GetTrackId());

			/*
			// Transcoder -> MediaRouter -> RelayClient
			// or
			// RelayServer -> MediaRouter -> RelayClient
			if (
				(connector_type == MediaRouteApplicationConnector::ConnectorType::Transcoder) ||
				(connector_type == MediaRouteApplicationConnector::ConnectorType::Relay))
			{
				_relay_server->SendMediaPacket(stream, cur_buf.get());
			}
			*/

			// Observer(Publisher or Transcoder)에게 MediaBuffer를 전달함.
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
						// logtd("send to publisher (1000k cr):%u, (90k cr):%u", cur_buf->GetPts(), encoded_frame->_timeStamp);
						observer->OnSendVideoFrame(stream_info, media_packet);
					}
					else if (media_packet->GetMediaType() == MediaType::Audio)
					{
						// logtd("send to publisher (1000k cr):%u, (90k cr):%u", cur_buf->GetPts(), encoded_frame->_timeStamp);
						observer->OnSendAudioFrame(stream_info, media_packet);
					}
				}
			}
		}
	}
}
