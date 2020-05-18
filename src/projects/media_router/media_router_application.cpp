//==============================================================================
//
//  MediaRouteApplication
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include <base/info/stream.h>
#include "media_router_application.h"
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
	logti("Created media route application. application id(%u), (%s)"
		, _application_info.GetId(), _application_info.GetName().CStr());

	_indicator.SetAlias(ov::String::FormatString("%s - Mediarouter Application Indicator", _application_info.GetName().CStr()));
}

MediaRouteApplication::~MediaRouteApplication()
{
	logti("Destroyed media router application. application id(%u), (%s)"
		, _application_info.GetId(), _application_info.GetName().CStr());
}

bool MediaRouteApplication::Start()
{
	try
	{
		_kill_flag = false;

		_thread = std::thread(&MediaRouteApplication::MessageLooper, this);
	}
	catch (const std::system_error &e)
	{
		_kill_flag = true;
		logte("Failed to start media route application thread.");
		return false;
	}

	return true;
}

bool MediaRouteApplication::Stop()
{
	_kill_flag = true;
	
	_indicator.Stop();
	_indicator.Clear();

	if(_thread.joinable())
	{
		_thread.join();
	}

	// TODO: Delete All Stream

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

	std::lock_guard<std::shared_mutex> lock(_connectors_lock);

	app_conn->SetMediaRouterApplication(GetSharedPtr());
	
	_connectors.push_back(app_conn);

	logti("Registered connector. %p app(%s) type(%d)"
		, app_conn.get(), _application_info.GetName().CStr(),  app_conn->GetConnectorType());

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
	
	std::lock_guard<std::shared_mutex> lock(_connectors_lock);

	auto position = std::find(_connectors.begin(), _connectors.end(), app_conn);
	if (position == _connectors.end())
	{
		return true;
	}

	_connectors.erase(position);

	logti("Unregistered connector. %p app(%s) type(%d)"
		, app_conn.get(), _application_info.GetName().CStr(),  app_conn->GetConnectorType());

	return true;
}

bool MediaRouteApplication::RegisterObserverApp(
	std::shared_ptr<MediaRouteApplicationObserver> app_obsrv)
{
	if (app_obsrv == nullptr)
	{
		return false;
	}

	std::lock_guard<std::shared_mutex> lock(_observers_lock);

	_observers.push_back(app_obsrv);

	logti("Registered observer. %p app(%s) type(%d)"
		, app_obsrv.get(), _application_info.GetName().CStr(), app_obsrv->GetObserverType());

	return true;
}

bool MediaRouteApplication::UnregisterObserverApp(
	std::shared_ptr<MediaRouteApplicationObserver> app_obsrv)
{
	if (app_obsrv == nullptr)
	{
		return false;
	}

	std::lock_guard<std::shared_mutex> lock(_observers_lock);

	auto position = std::find(_observers.begin(), _observers.end(), app_obsrv);
	if (position == _observers.end())
	{
		return true;
	}

	_observers.erase(position);

	logti("Unregistered observer. %p app(%s) type(%d)"
		, app_obsrv.get(), _application_info.GetName().CStr(),app_obsrv->GetObserverType());

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

	auto connector_type = app_conn->GetConnectorType();

	// If there is same stream, reuse that
	if (connector_type == MediaRouteApplicationConnector::ConnectorType::Provider)
	{
		std::lock_guard<std::shared_mutex> lock_guard(_streams_lock);

		for (auto it = _streams_incoming.begin(); it != _streams_incoming.end(); ++it)
		{
			auto istream = it->second;

			if (stream_info->GetName() == istream->GetStream()->GetName())
			{
				// reuse stream
				stream_info->SetId(istream->GetStream()->GetId());
				logtw("Reconnected same stream from provider(%s, %d)"
					, stream_info->GetName().CStr(), stream_info->GetId());

				return true;
			}
		}
	}

	logti("Trying to create a stream: [%s/%s(%u)]"
		, _application_info.GetName().CStr(), stream_info->GetName().CStr(), stream_info->GetId());

	auto new_stream = std::make_shared<MediaRouteStream>(stream_info);
	new_stream->SetConnectorType(connector_type);

	if(connector_type == MediaRouteApplicationConnector::ConnectorType::Provider)
	{
		std::lock_guard<std::shared_mutex> lock_guard(_streams_lock);
		new_stream->SetInoutType(false);
		_streams_incoming.insert(std::make_pair(stream_info->GetId(), new_stream));		
	}
	else if( (connector_type == MediaRouteApplicationConnector::ConnectorType::Transcoder) || 
			 (connector_type == MediaRouteApplicationConnector::ConnectorType::Relay) )
	{
		std::lock_guard<std::shared_mutex> lock_guard(_streams_lock);

		new_stream->SetInoutType(true);
		_streams_outgoing.insert(std::make_pair(stream_info->GetId(), new_stream));			
	}
	else
	{
		logte("Unsupported connector type %d", connector_type);

		return false;
	}

	// For Monitoring
	mon::Monitoring::GetInstance()->OnStreamCreated(*stream_info);

	// Notify all observers that a stream has been created
	{
		std::shared_lock<std::shared_mutex> lock(_observers_lock);

		for (auto observer : _observers)
		{
			auto oberver_type = observer->GetObserverType();

			if(connector_type == MediaRouteApplicationConnector::ConnectorType::Provider)
			{
				// Flow: Provider -> MediaRoute -> Transcoder
				if (oberver_type == MediaRouteApplicationObserver::ObserverType::Transcoder)
				{
					observer->OnCreateStream(new_stream->GetStream());
				}
				// Flow: Provider -> MediaRoute -> Relay
				else if (oberver_type == MediaRouteApplicationObserver::ObserverType::Relay)
				{
					observer->OnCreateStream(new_stream->GetStream());
				}
			}
			else if (connector_type == MediaRouteApplicationConnector::ConnectorType::Transcoder)
			{
				// Flow: Transcoder -> MediaRoute -> Publisher
				if(oberver_type == MediaRouteApplicationObserver::ObserverType::Publisher)
				{
					observer->OnCreateStream(new_stream->GetStream());
				}
			}
			else if(connector_type == MediaRouteApplicationConnector::ConnectorType::Relay)
			{
				// Flow: RelayClient -> MediaRoute -> Publisher
				if (oberver_type == MediaRouteApplicationObserver::ObserverType::Publisher)
				{
					observer->OnCreateStream(new_stream->GetStream());
				}
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

	logti("Trying to delete a stream: [%s/%s(%u)]"
		, _application_info.GetName().CStr(), stream_info->GetName().CStr(), stream_info->GetId());

	// For Monitoring
	mon::Monitoring::GetInstance()->OnStreamDeleted(*stream_info);

	logtd("Deleted connector. type(%d), app(%s) stream(%s/%u)"
		, app_conn->GetConnectorType(), _application_info.GetName().CStr(), stream_info->GetName().CStr(), stream_info->GetId());

	auto connector_type = app_conn->GetConnectorType();

	// Notify all observers that stream has been deleted
	{
		std::shared_lock<std::shared_mutex> lock_guard(_observers_lock);
		for (auto it = _observers.begin(); it != _observers.end(); ++it)
		{
			auto observer = *it;

			auto observer_type = observer->GetObserverType();

			if(connector_type ==  MediaRouteApplicationConnector::ConnectorType::Provider)
			{
				if(	(observer_type == MediaRouteApplicationObserver::ObserverType::Transcoder) ||
					(observer_type == MediaRouteApplicationObserver::ObserverType::Relay) ||
					(observer_type == MediaRouteApplicationObserver::ObserverType::Orchestrator))
				{
					observer->OnDeleteStream(stream_info);
				}
			}
			else if(connector_type ==  MediaRouteApplicationConnector::ConnectorType::Transcoder)
			{
				if ((observer_type == MediaRouteApplicationObserver::ObserverType::Publisher) ||
					(observer_type == MediaRouteApplicationObserver::ObserverType::Relay) ||
					(observer_type == MediaRouteApplicationObserver::ObserverType::Orchestrator))
				{
					observer->OnDeleteStream(stream_info);
				}
			}
			else if(connector_type ==  MediaRouteApplicationConnector::ConnectorType::Relay)
			{
				if(	(observer_type == MediaRouteApplicationObserver::ObserverType::Transcoder) ||
					(observer_type == MediaRouteApplicationObserver::ObserverType::Publisher) ||
					(observer_type == MediaRouteApplicationObserver::ObserverType::Orchestrator))
				{
					observer->OnDeleteStream(stream_info);
				}
			}
		}
	}

	if(connector_type == MediaRouteApplicationConnector::ConnectorType::Provider)
	{
		std::lock_guard<std::shared_mutex> lock_guard(_streams_lock);
		_streams_incoming.erase(stream_info->GetId());	
	}
	else if( (connector_type == MediaRouteApplicationConnector::ConnectorType::Transcoder) || 
			 (connector_type == MediaRouteApplicationConnector::ConnectorType::Relay) )
	{
		std::lock_guard<std::shared_mutex> lock_guard(_streams_lock);
		_streams_outgoing.erase(stream_info->GetId());	
	}
	else
	{
		logte("Unsupported connector type %d", app_conn->GetConnectorType());

		return false;
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

	uint8_t indicator = -BufferIndicator::BUFFER_INDICATOR_NONE_STREAM;

	std::shared_ptr<MediaRouteStream> stream = nullptr;

	{
		std::shared_lock<std::shared_mutex> lock_guard(_streams_lock);

		if(app_conn->GetConnectorType() == MediaRouteApplicationConnector::ConnectorType::Provider)
		{
			auto stream_bucket = _streams_incoming.find(stream_info->GetId());	
			if (stream_bucket == _streams_incoming.end())
			{
				logte("cannot find stream from router. appication(%s), stream(%s)"
					, _application_info.GetName().CStr(), stream_info->GetName().CStr());

				return false;
			}
			
			indicator = BufferIndicator::BUFFER_INDICATOR_INCOMING_STREAM;

			stream = stream_bucket->second;
		}
		else if( (app_conn->GetConnectorType() == MediaRouteApplicationConnector::ConnectorType::Transcoder) || 
				 (app_conn->GetConnectorType() == MediaRouteApplicationConnector::ConnectorType::Relay) )
		{
			auto stream_bucket = _streams_outgoing.find(stream_info->GetId());	
			if (stream_bucket == _streams_outgoing.end())
			{
				logte("cannot find stream from router. appication(%s), stream(%s)"
					, _application_info.GetName().CStr(), stream_info->GetName().CStr());

				return false;
			}
			
			indicator = BufferIndicator::BUFFER_INDICATOR_OUTGOING_STREAM;
			
			stream = stream_bucket->second;
		}
	}

	if (stream == nullptr)
	{
		logte("Not found stream");
		return false;
	}

	bool ret = stream->Push(packet);
	if(ret == true)
	{
		_indicator.Enqueue(std::make_shared<BufferIndicator>(indicator,stream_info->GetId()));
	}
	
	return ret;
}


void MediaRouteApplication::MessageLooper()
{
	while (!_kill_flag)
	{
		auto msg = _indicator.Dequeue(10);
		if (msg.has_value() == false)
		{
			// It may be called due to a normal stop signal.
			continue;
		}

		auto &indicator = msg.value();

		std::shared_ptr<MediaRouteStream> stream = nullptr;

		std::shared_lock<std::shared_mutex> lock(_streams_lock);
		if(indicator->_inout == BufferIndicator::BUFFER_INDICATOR_INCOMING_STREAM)
		{
			auto it = _streams_incoming.find(indicator->_stream_id);
			stream = (it != _streams_incoming.end()) ? it->second : nullptr;
		}
		else if(indicator->_inout == BufferIndicator::BUFFER_INDICATOR_OUTGOING_STREAM)
		{
			auto it = _streams_outgoing.find(indicator->_stream_id);
			stream = (it != _streams_outgoing.end()) ? it->second : nullptr;
		}
		lock.unlock();

		if (stream == nullptr)
		{
			logtw("Not found stream - strem_id(%u)", indicator->_stream_id);
			continue;
		}

		auto stream_info = stream->GetStream();

		while(auto media_packet = stream->Pop())
		{
			// Find Media Track
			auto media_track = stream_info->GetTrack(media_packet->GetTrackId());

			std::shared_lock<std::shared_mutex> lock(_observers_lock);

			// Deliver media packet to Publiser(observer) of Transcoder(observer)		
			for (const auto &observer : _observers)
			{
				auto observer_type = observer->GetObserverType();

				// Provider (from incoming stream) -> MediaRouter -> Transcoder
				if(indicator->_inout == BufferIndicator::BUFFER_INDICATOR_INCOMING_STREAM)
				{
					if(observer_type == MediaRouteApplicationObserver::ObserverType::Transcoder)
					{
						auto media_buffer_clone = media_packet->ClonePacket();

						observer->OnSendFrame(stream_info, std::move(media_buffer_clone));
					}
				}
				// Transcoder or RelayClient (from outgoing stream) -> MediaRouter -> Publisher
				else if(indicator->_inout == BufferIndicator::BUFFER_INDICATOR_OUTGOING_STREAM)
				{
					if(observer_type == MediaRouteApplicationObserver::ObserverType::Publisher)
					{
						if (media_packet->GetMediaType() == MediaType::Video)
						{
							observer->OnSendVideoFrame(stream_info, media_packet);
						}
						else if (media_packet->GetMediaType() == MediaType::Audio)
						{
							observer->OnSendAudioFrame(stream_info, media_packet);
						}
					}
				}
			}

			lock.unlock();
		}
	}
}
