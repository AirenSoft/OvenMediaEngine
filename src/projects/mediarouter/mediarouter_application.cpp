//==============================================================================
//
//  MediaRouteApplication
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include <base/info/stream.h>
#include "mediarouter_application.h"
#include "monitoring/monitoring.h"

#define OV_LOG_TAG "MediaRouter.App"

#define ASYNC_CSTREAM_ENABLE 0

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
	: _application_info(application_info),
	_indicator(nullptr, 100)
{
	logti("Created media route application. application id(%u), (%s)"
		, _application_info.GetId(), _application_info.GetName().CStr());

	_indicator.SetAlias(ov::String::FormatString("%s - MediaRouter Indicator Queue", _application_info.GetName().CStr()));
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
	std::lock_guard<std::shared_mutex> lock(_connectors_lock);

	if (app_conn == nullptr)
	{
		return false;
	}

	app_conn->SetMediaRouterApplication(GetSharedPtr());
	
	_connectors.push_back(app_conn);

	logtd("Registered connector. %p app(%s) type(%d)"
		, app_conn.get(), _application_info.GetName().CStr(),  app_conn->GetConnectorType());

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

	logti("Unregistered connector. %p app(%s) type(%d)"
		, app_conn.get(), _application_info.GetName().CStr(),  app_conn->GetConnectorType());

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

	logtd("Registered observer. %p app(%s) type(%d)"
		, app_obsrv.get(), _application_info.GetName().CStr(), app_obsrv->GetObserverType());

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

	logti("Unregistered observer. %p app(%s) type(%d)"
		, app_obsrv.get(), _application_info.GetName().CStr(),app_obsrv->GetObserverType());

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
	logti("Trying to create a stream: [%s/%s(%u)]"
		, _application_info.GetName().CStr(), stream_info->GetName().CStr(), stream_info->GetId());
	logti("%s", stream_info->GetInfoString().CStr());
	

	auto connector = app_conn->GetConnectorType();

	// If there is same stream, reuse that
	if (connector == MediaRouteApplicationConnector::ConnectorType::Provider)
	{
		if(ReuseIncomingStream(stream_info))
			return true;
	}

	if(connector == MediaRouteApplicationConnector::ConnectorType::Provider)
	{
		if(!CreateIncomingStream(stream_info))
			return false;
	}
	else if(connector == MediaRouteApplicationConnector::ConnectorType::Transcoder)
	{
		if(!CreateOutgoingStream(stream_info))
			return false;
	}
	else if(connector == MediaRouteApplicationConnector::ConnectorType::Relay)
	{
		if(!CreateOutgoingStream(stream_info))
			return false;
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
	//              Because, after the track information parsing in the incoming stream, 
	// 			    Notifies the Observer that streaming has been created.
	// - from Transcoder : Notify to Observer(Publisher)
	// - from Relay : Notify to Observer(Publisher)

#if ASYNC_CSTREAM_ENABLE
	if(connector == MediaRouteApplicationConnector::ConnectorType::Provider)
	{
		return true;
	}
#endif

	if(!NotifyCreateStream(stream_info, connector))
	{
		return false;
	}


	return true;
}


bool MediaRouteApplication::ReuseIncomingStream(
	const std::shared_ptr<info::Stream> &stream_info)
{
	std::lock_guard<std::shared_mutex> lock_guard(_streams_lock);

	for (auto it = _streams_incoming.begin(); it != _streams_incoming.end(); ++it)
	{
		auto istream = it->second;

		if (stream_info->GetName() == istream->GetStream()->GetName())
		{
			// reuse stream
			stream_info->SetId(istream->GetStream()->GetId());
			logtw("Reconnected same stream from provider(%s, %d)", 
				stream_info->GetName().CStr(), stream_info->GetId());

			return true;
		}
	}	

	return false;
}


bool MediaRouteApplication::CreateIncomingStream(
	const std::shared_ptr<info::Stream> &stream_info)
{
	std::lock_guard<std::shared_mutex> lock_guard(_streams_lock);

	auto new_stream = std::make_shared<MediaRouteStream>(stream_info, MRStreamInoutType::Incoming);
	if(!new_stream)
		return false;

	_streams_incoming.insert(std::make_pair(stream_info->GetId(), new_stream));	

	return true;	
}


bool MediaRouteApplication::CreateOutgoingStream(
	const std::shared_ptr<info::Stream> &stream_info)
{
	std::lock_guard<std::shared_mutex> lock_guard(_streams_lock);

	auto new_stream = std::make_shared<MediaRouteStream>(stream_info, MRStreamInoutType::Outgoing);
	if(!new_stream)
		return false;
	
	_streams_outgoing.insert(std::make_pair(stream_info->GetId(), new_stream));	

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

		switch(connector_type)
		{
			case MediaRouteApplicationConnector::ConnectorType::Provider:
			{
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
			} break;

			case MediaRouteApplicationConnector::ConnectorType::Transcoder:
			case MediaRouteApplicationConnector::ConnectorType::Relay:
			{
				if(oberver_type == MediaRouteApplicationObserver::ObserverType::Publisher)
				{
					observer->OnCreateStream(stream_info);
				}
			} break;
		}
	}


	return true;
}


bool MediaRouteApplication::OnDeleteStream(
	const std::shared_ptr<MediaRouteApplicationConnector> &app_conn,
	const std::shared_ptr<info::Stream> &stream_info)
{
	logti("Trying to delete a stream: [%s/%s(%u)]"
		, _application_info.GetName().CStr(), stream_info->GetName().CStr(), stream_info->GetId());

	if (!app_conn  || !stream_info)
	{
		logte("Invalid arguments: connector: %p, stream: %p", app_conn.get(), stream_info.get());
		return false;
	}

	// For Monitoring
	mon::Monitoring::GetInstance()->OnStreamDeleted(*stream_info);

	if(!NotifyDeleteStream(stream_info, app_conn->GetConnectorType()))
	{
		return false;
	}

	switch(app_conn->GetConnectorType())
	{
		case MediaRouteApplicationConnector::ConnectorType::Provider:
		{
			DeleteIncomingStream(stream_info);
		} break;

		case MediaRouteApplicationConnector::ConnectorType::Transcoder:
		case MediaRouteApplicationConnector::ConnectorType::Relay:
		{
			DeleteOutgoingStream(stream_info);
		}
		break;
		default:
			logte("Unknown connector type %d", app_conn->GetConnectorType());
		break;
	}

	return true;
}

bool MediaRouteApplication::DeleteIncomingStream(
	const std::shared_ptr<info::Stream> &stream_info)
{
	std::lock_guard<std::shared_mutex> lock_guard(_streams_lock);
	
	_streams_incoming.erase(stream_info->GetId());	

	return true;
}
bool MediaRouteApplication::DeleteOutgoingStream(
	const std::shared_ptr<info::Stream> &stream_info)
{
	std::lock_guard<std::shared_mutex> lock_guard(_streams_lock);
	
	_streams_outgoing.erase(stream_info->GetId());

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

	auto indicator = -BufferIndicator::BUFFER_INDICATOR_NONE_STREAM;

	switch(app_conn->GetConnectorType())
	{
		case MediaRouteApplicationConnector::ConnectorType::Provider:
		{
			indicator = BufferIndicator::BUFFER_INDICATOR_INCOMING_STREAM;

		} break;	

		case MediaRouteApplicationConnector::ConnectorType::Transcoder:
		case MediaRouteApplicationConnector::ConnectorType::Relay:
		{
			indicator = BufferIndicator::BUFFER_INDICATOR_OUTGOING_STREAM;
		} break;
		default:
		{
			logte("Unknown connector type");
			return false;		
		}
	}

	auto stream = GetStream(indicator, stream_info->GetId());
	if(!stream)
	{
		logte("cannot find stream from router. appication(%s), stream(%s)"
				, _application_info.GetName().CStr(), stream_info->GetName().CStr());
		return false;
	}

	// Put packets in stream queues, add an indicator for packet processing.
	if(!stream->Push(packet))
	{
		return false;
	}

	// When the incoming stream is finished parsing track information, 
	// Notify the Observer that the stream is created.
#if ASYNC_CSTREAM_ENABLE
	if(stream->GetInoutType() == MRStreamInoutType::Incoming)
	{
		if(stream->IsCreatedSteam() == false && stream->IsParseTrackAll() == true)
		{
			//logtw("NotifyCreateStream(stream->GetStream(),app_conn->GetConnectorType())");
			NotifyCreateStream(stream->GetStream(),app_conn->GetConnectorType());
			stream->SetCreatedSteam(true);
		}
	}
#endif	
	_indicator.Enqueue(std::make_shared<BufferIndicator>(indicator, stream_info->GetId()));

	return true;
}

std::shared_ptr<MediaRouteStream> MediaRouteApplication::GetStream(
	uint8_t indicator, 
	uint32_t stream_id)
{
	std::shared_lock<std::shared_mutex> lock_guard(_streams_lock);

	if(indicator == BufferIndicator::BUFFER_INDICATOR_INCOMING_STREAM)
	{
		auto stream_bucket = _streams_incoming.find(stream_id);	
		if (stream_bucket == _streams_incoming.end())
		{
			return nullptr;
		}

		return stream_bucket->second;
	}
	else if(indicator == BufferIndicator::BUFFER_INDICATOR_OUTGOING_STREAM)
	{
		auto stream_bucket = _streams_outgoing.find(stream_id);	
		if (stream_bucket == _streams_outgoing.end())
		{
			return nullptr;
		}
		
		return stream_bucket->second;		
	}

	return nullptr;
}

void MediaRouteApplication::MessageLooper()
{
	while (!_kill_flag)
	{
		auto msg = _indicator.Dequeue(100);
		if (msg.has_value() == false)
		{
			// It may be called due to a normal stop signal.
			continue;
		}
		auto &indicator = msg.value();

		// Get MediaRouter Stream
		std::shared_ptr<MediaRouteStream> stream = nullptr;

		std::shared_lock<std::shared_mutex> lock(_streams_lock);
		switch(indicator->_inout)
		{
			case BufferIndicator::BUFFER_INDICATOR_INCOMING_STREAM:
			{
				auto it = _streams_incoming.find(indicator->_stream_id);
				stream = (it != _streams_incoming.end()) ? it->second : nullptr;
			} break;

			case BufferIndicator::BUFFER_INDICATOR_OUTGOING_STREAM:
			{
				auto it = _streams_outgoing.find(indicator->_stream_id);
				stream = (it != _streams_outgoing.end()) ? it->second : nullptr;			
			} break;
			
			case BufferIndicator::BUFFER_INDICATOR_NONE_STREAM:
			default:
			{
				continue; 
			} break;
		}
		lock.unlock();

		if (stream == nullptr)
		{
			logtw("Not found stream - strem_id(%u)", indicator->_stream_id);
			continue;
		}

		// Get Stream Info
		auto stream_info = stream->GetStream();

		// Processes all packets in the selected stream.
		// StreamDeliver media packet to Publiser(observer) of Transcoder(observer)		
		while(auto media_packet = stream->Pop())
		{
			// Find Media Track
			auto media_track = stream_info->GetTrack(media_packet->GetTrackId());
			if(media_track == nullptr)
			{
				continue;
			}

			std::shared_lock<std::shared_mutex> lock(_observers_lock);
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
						observer->OnSendFrame(stream_info, media_packet);
					}
				}
			}
			lock.unlock();
		}
	}

	logtd("MessageLooper thread has been stopped");
}
