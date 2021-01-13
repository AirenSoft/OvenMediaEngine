//==============================================================================
//
//  MediaRouteApplication
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "mediarouter_application.h"

#include <base/info/stream.h>

#include "mediarouter_private.h"
#include "monitoring/monitoring.h"

#define ASYNC_CSTREAM_ENABLE 0
#define MIN_APPLICATION_WORKER_COUNT 1
#define MAX_APPLICATION_WORKER_COUNT 72
using namespace cmn;

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
	_max_worker_thread_count = _application_info.GetConfig().GetPublishers().GetStreamLoadBalancingThreadCount();
	if (_max_worker_thread_count < MIN_APPLICATION_WORKER_COUNT)
	{
		_max_worker_thread_count = MIN_APPLICATION_WORKER_COUNT;
	}
	if (_max_worker_thread_count > MAX_APPLICATION_WORKER_COUNT)
	{
		_max_worker_thread_count = MAX_APPLICATION_WORKER_COUNT;
	}

	logti("Created Mediarouter application. application id(%u), app(%s), worker(%d)", _application_info.GetId(), _application_info.GetName().CStr(), _max_worker_thread_count);

	for (uint32_t worker_id = 0; worker_id < _max_worker_thread_count; worker_id++)
	{
		_inbound_stream_indicator.push_back(std::make_shared<ov::Queue<std::shared_ptr<MediaRouteStream>>>(
			ov::String::FormatString("%s - Mediarouter inbound indicator (%d/%d)", _application_info.GetName().CStr(), worker_id, _max_worker_thread_count),
			100));

		_outbound_stream_indicator.push_back(std::make_shared<ov::Queue<std::shared_ptr<MediaRouteStream>>>(
			ov::String::FormatString("%s - Mediarouter outbound indicator (%d/%d)", _application_info.GetName().CStr(), worker_id, _max_worker_thread_count),
			100));
	}
}

MediaRouteApplication::~MediaRouteApplication()
{
	logti("Destroyed Mediarouter application. application id(%u), app(%s)", _application_info.GetId(), _application_info.GetName().CStr());
}

bool MediaRouteApplication::Start()
{
	_kill_flag = false;

	for (uint32_t worker_id = 0; worker_id < _max_worker_thread_count; worker_id++)
	{
		try
		{
			auto inbound_thread = std::thread(&MediaRouteApplication::InboundWorkerThread, this, worker_id);
			pthread_setname_np(inbound_thread.native_handle(), "InboundWorker");
			_inbound_threads.push_back(std::move(inbound_thread));
		}
		catch (const std::system_error &e)
		{
			_kill_flag = true;
			logte("Failed to start Mediarouter application thread.");
			return false;
		}
	}

	for (uint32_t worker_id = 0; worker_id < _max_worker_thread_count; worker_id++)
	{
		try
		{
			auto outbound_thread = std::thread(&MediaRouteApplication::OutboundWorkerThread, this, worker_id);
			pthread_setname_np(outbound_thread.native_handle(), "OutboundWorker");
			_outbound_threads.push_back(std::move(outbound_thread));
		}
		catch (const std::system_error &e)
		{
			_kill_flag = true;
			logte("Failed to start Mediarouter application thread.");
			return false;
		}
	}

	logti("Started Mediarouter application. application id(%u), app(%s)", _application_info.GetId(), _application_info.GetName().CStr());

	return true;
}

bool MediaRouteApplication::Stop()
{
	_kill_flag = true;

	for (auto &indicator : _inbound_stream_indicator)
	{
		indicator->Stop();
		indicator->Clear();
	}

	for (auto &indicator : _outbound_stream_indicator)
	{
		indicator->Stop();
		indicator->Clear();
	}

	for (auto &worker : _inbound_threads)
	{
		if (worker.joinable())
		{
			worker.join();
		}
	}

	for (auto &worker : _outbound_threads)
	{
		if (worker.joinable())
		{
			worker.join();
		}
	}

	_inbound_stream_indicator.clear();
	_outbound_stream_indicator.clear();
	_inbound_threads.clear();
	_outbound_threads.clear();

	// TODO: Delete All Stream
	_connectors.clear();
	_observers.clear();

	logti("Mediarouter application. id(%u), app(%s) has been stopped", _application_info.GetId(), _application_info.GetName().CStr());

	return true;
}

// Called when an application is created
bool MediaRouteApplication::RegisterConnectorApp(
	std::shared_ptr<MediaRouteApplicationConnector> app_conn)
{
	std::lock_guard<std::shared_mutex> lock(_connectors_lock);

	if (app_conn == nullptr)
	{
		return false;
	}

	app_conn->SetMediaRouterApplication(GetSharedPtr());

	_connectors.push_back(app_conn);

	logtd("Registered connector. %p app(%s) type(%d)", app_conn.get(), _application_info.GetName().CStr(), app_conn->GetConnectorType());

	return true;
}

// Called when an application is remvoed
bool MediaRouteApplication::UnregisterConnectorApp(
	std::shared_ptr<MediaRouteApplicationConnector> app_conn)
{
	std::lock_guard<std::shared_mutex> lock(_connectors_lock);

	if (!app_conn)
	{
		return false;
	}

	auto position = std::find(_connectors.begin(), _connectors.end(), app_conn);
	if (position == _connectors.end())
	{
		return true;
	}

	_connectors.erase(position);

	logti("Unregistered connector. %p app(%s) type(%d)", app_conn.get(), _application_info.GetName().CStr(), app_conn->GetConnectorType());

	return true;
}

bool MediaRouteApplication::RegisterObserverApp(
	std::shared_ptr<MediaRouteApplicationObserver> app_obsrv)
{
	std::lock_guard<std::shared_mutex> lock(_observers_lock);

	if (!app_obsrv)
	{
		return false;
	}

	_observers.push_back(app_obsrv);

	logtd("Registered observer. %p app(%s) type(%d)", app_obsrv.get(), _application_info.GetName().CStr(), app_obsrv->GetObserverType());

	return true;
}

bool MediaRouteApplication::UnregisterObserverApp(
	std::shared_ptr<MediaRouteApplicationObserver> app_obsrv)
{
	std::lock_guard<std::shared_mutex> lock(_observers_lock);

	if (!app_obsrv)
	{
		return false;
	}

	auto position = std::find(_observers.begin(), _observers.end(), app_obsrv);
	if (position == _observers.end())
	{
		return true;
	}

	_observers.erase(position);

	logti("Unregistered observer. %p app(%s) type(%d)", app_obsrv.get(), _application_info.GetName().CStr(), app_obsrv->GetObserverType());

	return true;
}

// OnCreateStream is called from Provider, Transcoder, Relay
bool MediaRouteApplication::OnCreateStream(
	const std::shared_ptr<MediaRouteApplicationConnector> &app_conn,
	const std::shared_ptr<info::Stream> &stream_info)
{
	if (!app_conn || !stream_info)
	{
		OV_ASSERT2(false);
		return false;
	}

	logti("Trying to create a stream: [%s/%s(%u)]", _application_info.GetName().CStr(), stream_info->GetName().CStr(), stream_info->GetId());
	logti("%s", stream_info->GetInfoString().CStr());

	auto connector = app_conn->GetConnectorType();

	if (connector == MediaRouteApplicationConnector::ConnectorType::Provider)
	{
		// Check there is a duplicate inbound stream
		if (IsExistingInboundStream(stream_info->GetName()) == true)
		{
			logtw("Reject stream creation : there is already an incoming stream with the same name. (%s)", stream_info->GetName().CStr());
			return false;
		}

		if (!CreateInboundStream(stream_info))
		{
			return false;
		}
	}
	else if (connector == MediaRouteApplicationConnector::ConnectorType::Transcoder)
	{
		if (!CreateOutboundStream(stream_info))
		{
			return false;
		}
	}
	else if (connector == MediaRouteApplicationConnector::ConnectorType::Relay)
	{
		if (!CreateOutboundStream(stream_info))
		{
			return false;
		}
	}
	else
	{
		logte("Unsupported connector type %d", connector);

		return false;
	}

	// For Monitoring
	mon::Monitoring::GetInstance()->OnStreamCreated(*stream_info);

	// Create Stream Flow
	// - from Provider : The Observer is not notified that a stream has been created.
	//              Because, after the track information parsing in the inbound stream,
	// 			    Notifies the Observer that streaming has been created.
	// - from Transcoder : Notify to Observer(Publisher)
	// - from Relay : Notify to Observer(Publisher)

#if ASYNC_CSTREAM_ENABLE
	if (connector == MediaRouteApplicationConnector::ConnectorType::Provider)
	{
		return true;
	}
#endif

	if (!NotifyCreateStream(stream_info, connector))
	{
		return false;
	}

	return true;
}

bool MediaRouteApplication::CreateInboundStream(
	const std::shared_ptr<info::Stream> &stream_info)
{
	std::lock_guard<std::shared_mutex> lock_guard(_streams_lock);

	auto new_stream = std::make_shared<MediaRouteStream>(stream_info, MediaRouterStreamType::INBOUND);
	if (!new_stream)
		return false;

	_inbound_streams.insert(std::make_pair(stream_info->GetId(), new_stream));

	return true;
}

bool MediaRouteApplication::CreateOutboundStream(
	const std::shared_ptr<info::Stream> &stream_info)
{
	std::lock_guard<std::shared_mutex> lock_guard(_streams_lock);

	auto new_stream = std::make_shared<MediaRouteStream>(stream_info, MediaRouterStreamType::OUTBOUND);
	if (!new_stream)
		return false;

	_outbound_streams.insert(std::make_pair(stream_info->GetId(), new_stream));

	return true;
}

bool MediaRouteApplication::NotifyCreateStream(
	const std::shared_ptr<info::Stream> &stream_info,
	MediaRouteApplicationConnector::ConnectorType connector_type)
{
	std::shared_lock<std::shared_mutex> lock(_observers_lock);

	for (auto observer : _observers)
	{
		auto oberver_type = observer->GetObserverType();

		switch (connector_type)
		{
			case MediaRouteApplicationConnector::ConnectorType::Provider: {
				// Flow: Provider -> MediaRoute -> Transcoder
				if (oberver_type == MediaRouteApplicationObserver::ObserverType::Transcoder)
				{
					observer->OnCreateStream(stream_info);
				}
				// Flow: Provider -> MediaRoute -> Relay
				else if (oberver_type == MediaRouteApplicationObserver::ObserverType::Relay)
				{
					observer->OnCreateStream(stream_info);
				}
			}
			break;

			case MediaRouteApplicationConnector::ConnectorType::Transcoder:
			case MediaRouteApplicationConnector::ConnectorType::Relay: {
				if (oberver_type == MediaRouteApplicationObserver::ObserverType::Publisher)
				{
					observer->OnCreateStream(stream_info);
				}
			}
			break;
		}
	}

	return true;
}

bool MediaRouteApplication::OnDeleteStream(
	const std::shared_ptr<MediaRouteApplicationConnector> &app_conn,
	const std::shared_ptr<info::Stream> &stream_info)
{
	logti("Trying to delete a stream: [%s/%s(%u)]", _application_info.GetName().CStr(), stream_info->GetName().CStr(), stream_info->GetId());

	if (!app_conn || !stream_info)
	{
		logte("Invalid arguments: connector: %p, stream: %p", app_conn.get(), stream_info.get());
		return false;
	}

	// For Monitoring
	mon::Monitoring::GetInstance()->OnStreamDeleted(*stream_info);

	if (!NotifyDeleteStream(stream_info, app_conn->GetConnectorType()))
	{
		return false;
	}

	switch (app_conn->GetConnectorType())
	{
		case MediaRouteApplicationConnector::ConnectorType::Provider: {
			DeleteInboundStream(stream_info);
		}
		break;

		case MediaRouteApplicationConnector::ConnectorType::Transcoder:
		case MediaRouteApplicationConnector::ConnectorType::Relay: {
			DeleteOutboundStream(stream_info);
		}
		break;
		default:
			logte("Unknown connector type %d", app_conn->GetConnectorType());
			break;
	}

	return true;
}

bool MediaRouteApplication::DeleteInboundStream(
	const std::shared_ptr<info::Stream> &stream_info)
{
	std::lock_guard<std::shared_mutex> lock_guard(_streams_lock);

	_inbound_streams.erase(stream_info->GetId());

	return true;
}
bool MediaRouteApplication::DeleteOutboundStream(
	const std::shared_ptr<info::Stream> &stream_info)
{
	std::lock_guard<std::shared_mutex> lock_guard(_streams_lock);

	_outbound_streams.erase(stream_info->GetId());

	return true;
}

bool MediaRouteApplication::NotifyDeleteStream(
	const std::shared_ptr<info::Stream> &stream_info,
	const MediaRouteApplicationConnector::ConnectorType connector_type)
{
	std::shared_lock<std::shared_mutex> lock_guard(_observers_lock);
	for (auto it = _observers.begin(); it != _observers.end(); ++it)
	{
		auto observer = *it;

		auto observer_type = observer->GetObserverType();

		if (connector_type == MediaRouteApplicationConnector::ConnectorType::Provider)
		{
			if ((observer_type == MediaRouteApplicationObserver::ObserverType::Transcoder) ||
				(observer_type == MediaRouteApplicationObserver::ObserverType::Relay) ||
				(observer_type == MediaRouteApplicationObserver::ObserverType::Orchestrator))
			{
				observer->OnDeleteStream(stream_info);
			}
		}
		else if (connector_type == MediaRouteApplicationConnector::ConnectorType::Transcoder)
		{
			if ((observer_type == MediaRouteApplicationObserver::ObserverType::Publisher) ||
				(observer_type == MediaRouteApplicationObserver::ObserverType::Relay) ||
				(observer_type == MediaRouteApplicationObserver::ObserverType::Orchestrator))
			{
				observer->OnDeleteStream(stream_info);
			}
		}
		else if (connector_type == MediaRouteApplicationConnector::ConnectorType::Relay)
		{
			if ((observer_type == MediaRouteApplicationObserver::ObserverType::Transcoder) ||
				(observer_type == MediaRouteApplicationObserver::ObserverType::Publisher) ||
				(observer_type == MediaRouteApplicationObserver::ObserverType::Orchestrator))
			{
				observer->OnDeleteStream(stream_info);
			}
		}
	}

	return true;
}

// @from Provider
// @from TranscoderProvider
bool MediaRouteApplication::OnReceiveBuffer(
	const std::shared_ptr<MediaRouteApplicationConnector> &app_conn,
	const std::shared_ptr<info::Stream> &stream_info,
	const std::shared_ptr<MediaPacket> &packet)
{
	if (!app_conn || !stream_info)
	{
		return false;
	}

	switch (app_conn->GetConnectorType())
	{
		case MediaRouteApplicationConnector::ConnectorType::Provider: {
			auto stream = GetInboundStream(stream_info->GetId());
			if (!stream)
			{
				logte("Could not foun inbound stream. name(%s/%s)", _application_info.GetName().CStr(), stream_info->GetName().CStr());
				return false;
			}

			if (!stream->Push(packet))
			{
				return false;
			}
			_inbound_stream_indicator[stream_info->GetId() % _max_worker_thread_count]->Enqueue(stream);
		}
		break;

		case MediaRouteApplicationConnector::ConnectorType::Transcoder:
		case MediaRouteApplicationConnector::ConnectorType::Relay: {
			auto stream = GetOutboundStream(stream_info->GetId());
			if (!stream)
			{
				logte("Could not found outbound stream. name(%s/%s)", _application_info.GetName().CStr(), stream_info->GetName().CStr());
				return false;
			}

			if (!stream->Push(packet))
			{
				return false;
			}
			_outbound_stream_indicator[stream_info->GetId() % _max_worker_thread_count]->Enqueue(stream);
		}
		break;
		default: {
			logte("Unknown connector type");
			return false;
		}
	}

		// When the inbound stream is finished parsing track information,
		// Notify the Observer that the stream is created.
#if ASYNC_CSTREAM_ENABLE
	if (stream->GetInoutType() == MediaRouterStreamType::INBOUND)
	{
		if (stream->IsCreatedSteam() == false && stream->IsParseTrackAll() == true)
		{
			//logtw("NotifyCreateStream(stream->GetStream(),app_conn->GetConnectorType())");
			NotifyCreateStream(stream->GetStream(), app_conn->GetConnectorType());
			stream->SetCreatedSteam(true);
		}
	}
#endif

	return true;
}

std::shared_ptr<MediaRouteStream> MediaRouteApplication::GetInboundStream(uint32_t stream_id)
{
	std::shared_lock<std::shared_mutex> lock_guard(_streams_lock);

	auto bucket = _inbound_streams.find(stream_id);
	if (bucket == _inbound_streams.end())
	{
		return nullptr;
	}

	return bucket->second;
}

std::shared_ptr<MediaRouteStream> MediaRouteApplication::GetOutboundStream(uint32_t stream_id)
{
	std::shared_lock<std::shared_mutex> lock_guard(_streams_lock);

	auto bucket = _outbound_streams.find(stream_id);
	if (bucket == _outbound_streams.end())
	{
		return nullptr;
	}

	return bucket->second;
}

bool MediaRouteApplication::IsExistingInboundStream(ov::String stream_name)
{
	std::shared_lock<std::shared_mutex> lock_guard(_streams_lock);

	for (const auto &item : _inbound_streams)
	{
		auto stream = item.second;
		if (stream->GetStream()->GetName() == stream_name)
		{
			return true;
		}
	}

	return false;
}

void MediaRouteApplication::InboundWorkerThread(uint32_t worker_id)
{
	logtd("Created Inbound worker thread #%d", worker_id);

	while (!_kill_flag)
	{
		auto msg = _inbound_stream_indicator[worker_id]->Dequeue(ov::Infinite);
		if (msg.has_value() == false)
		{
			// It may be called due to a normal stop signal.
			continue;
		}
		auto stream = msg.value();
		if (stream == nullptr)
		{
			logtw("Not found stream info");
			continue;
		}

		// Processes all packets in the selected stream.
		// StreamDeliver media packet to Publiser(observer) of Transcoder(observer)
		while (auto media_packet = stream->Pop())
		{
			std::shared_lock<std::shared_mutex> lock(_observers_lock);
			for (const auto &observer : _observers)
			{
				auto observer_type = observer->GetObserverType();

				if (observer_type == MediaRouteApplicationObserver::ObserverType::Transcoder)
				{
					// Get Stream Info
					auto stream_info = stream->GetStream();

					// observer->OnSendFrame(stream_info, std::move(media_packet->ClonePacket()));
					observer->OnSendFrame(stream_info, media_packet);
				}
			}
		}
	}

	logtd("Inbound worker thread #%d has beed stopped", worker_id);
}

void MediaRouteApplication::OutboundWorkerThread(uint32_t worker_id)
{
	logtd("Created outbound worker thread #%d", worker_id);

	while (!_kill_flag)
	{
		auto msg = _outbound_stream_indicator[worker_id]->Dequeue(ov::Infinite);
		if (msg.has_value() == false)
		{
			// It may be called due to a normal stop signal.
			continue;
		}
		auto stream = msg.value();
		if (stream == nullptr)
		{
			logtw("Not found stream info");
			continue;
		}

		// Processes all packets in the selected stream.
		// StreamDeliver media packet to Publiser(observer) of Transcoder(observer)
		while (auto media_packet = stream->Pop())
		{
			std::shared_lock<std::shared_mutex> lock(_observers_lock);
			for (const auto &observer : _observers)
			{
				auto observer_type = observer->GetObserverType();

				if (observer_type == MediaRouteApplicationObserver::ObserverType::Publisher)
				{
					// Get Stream Info
					auto stream_info = stream->GetStream();

					observer->OnSendFrame(stream_info, media_packet);
				}
			}
		}
	}

	logtd("Outbound worker thread #%d has beed stopped", worker_id);
}
