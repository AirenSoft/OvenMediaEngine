//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#include "rtmp_server.h"

#include <base/ovlibrary/ovlibrary.h>
#include <monitoring/monitoring.h>

#include "rtmp_provider_private.h"

RtmpServer::~RtmpServer()
{
	OV_ASSERT2(_physical_port == nullptr);
}

bool RtmpServer::Start(const ov::SocketAddress &address)
{
	if (_physical_port != nullptr)
	{
		logtw("RTMP server is already running");
		return false;
	}

	_physical_port = PhysicalPortManager::Instance()->CreatePort(ov::SocketType::Tcp, address);

	if (_physical_port == nullptr)
	{
		logte("Could not initialize phyiscal port for RTMP server: %s", address.ToString().CStr());

		return false;
	}

	_physical_port->AddObserver(this);

	// Setting the gargabe checking timer
	_garbage_check_timer.Push(std::bind(&RtmpServer::OnGarbageCheck, this, std::placeholders::_1), 3000);

	// Start the timer
	_garbage_check_timer.Start();

	_stat_stop_watch.Start();

	return true;
}

bool RtmpServer::Stop()
{
	if (_physical_port == nullptr)
	{
		logtw("RTMP server is not running");
		return false;
	}

	// Stop the garbage checking timer
	_garbage_check_timer.Stop();

	_physical_port->RemoveObserver(this);
	PhysicalPortManager::Instance()->DeletePort(_physical_port);

	_physical_port = nullptr;

	return true;
}

bool RtmpServer::AddObserver(const std::shared_ptr<RtmpObserver> &observer)
{
	logtd("Trying to add an observer %p with the RTMP Server", observer.get());

	std::unique_lock<std::shared_mutex> lock(_observers_lock);

	// Verify that the observer is already registered
	auto item = std::find_if(_observers.begin(), _observers.end(), [&](std::shared_ptr<RtmpObserver> const &value) -> bool {
		return value == observer;
	});

	if (item != _observers.end())
	{
		logtw("The observer %p is already registered with RTMP Server", observer.get());
		return false;
	}

	_observers.push_back(observer);

	return true;
}

bool RtmpServer::RemoveObserver(const std::shared_ptr<RtmpObserver> &observer)
{
	std::unique_lock<std::shared_mutex> lock(_observers_lock);

	auto item = std::find_if(_observers.begin(), _observers.end(), [&](std::shared_ptr<RtmpObserver> const &value) -> bool {
		return value == observer;
	});

	if (item == _observers.end())
	{
		logtw("The observer %p is not registered with RTMP Server", observer.get());
		return false;
	}

	_observers.erase(item);

	return true;
}

bool RtmpServer::Disconnect(const ov::String &app_name)
{
	std::unique_lock<std::recursive_mutex> lock(_chunk_context_list_mutex);

	//The item will be deleted in DisconnectClient()
	auto context_list = _chunk_context_list;
	for (auto item = context_list.begin(); item != context_list.end(); ++item)
	{
		auto &chunk_stream = item->second;
		if (chunk_stream->GetAppName() == app_name)
		{
			_physical_port->DisconnectClient(dynamic_cast<ov::ClientSocket *>(item->first));
		}
	}

	return true;
}

bool RtmpServer::Disconnect(const ov::String &app_name, uint32_t stream_id)
{
	std::unique_lock<std::recursive_mutex> lock(_chunk_context_list_mutex);

	for (auto item = _chunk_context_list.begin(); item != _chunk_context_list.end(); ++item)
	{
		auto &chunk_stream = item->second;

		if (chunk_stream->GetAppName() == app_name && chunk_stream->GetStreamId() == stream_id)
		{
			_physical_port->DisconnectClient(dynamic_cast<ov::ClientSocket *>(item->first));
			return true;
		}
	}

	return false;
}

void RtmpServer::OnConnected(const std::shared_ptr<ov::Socket> &remote)
{
	logti("A RTMP client has connected from %s", remote->ToString().CStr());

	std::unique_lock<std::recursive_mutex> lock(_chunk_context_list_mutex);

	_chunk_context_list.emplace(remote.get(), std::make_shared<RtmpChunkStream>(dynamic_cast<ov::ClientSocket *>(remote.get()), this));
}

void RtmpServer::OnDataReceived(const std::shared_ptr<ov::Socket> &remote, const ov::SocketAddress &address, const std::shared_ptr<const ov::Data> &data)
{
	if (_stat_stop_watch.IsElapsed(5000) && _stat_stop_watch.Update())
	{
		logts("Stats for RTMP socket %s: %s", remote->ToString().CStr(), remote->GetStat().CStr());
	}

	std::unique_lock<std::recursive_mutex> lock(_chunk_context_list_mutex);

	auto item = _chunk_context_list.find(remote.get());

	if (item != _chunk_context_list.end())
	{
		auto &chunk_stream = item->second;

		if (remote->GetState() != ov::SocketState::Connected)
		{
			logte("A data received from disconnected client: [%s/%s] %s", chunk_stream->GetAppName().CStr(), chunk_stream->GetStreamName().CStr(), remote->ToString().CStr());
			return;
		}

		auto current_data = data->Clone();
		bool succeeded = true;

		while (current_data->IsEmpty() == false)
		{
			int32_t parsed_bytes = chunk_stream->OnDataReceived(data);

			if (parsed_bytes < 0)
			{
				succeeded = false;
				break;
			}

			current_data = current_data->Subdata(parsed_bytes);

			if (parsed_bytes == 0)
			{
				OV_ASSERT2(current_data->IsEmpty());
				break;
			}
		}

		if (succeeded == false)
		{
			logte("An error occurred while process the RTMP packet: [%s/%s] (%u/%u), remote: %s, Disconnecting...",
				  chunk_stream->GetAppName().CStr(), chunk_stream->GetStreamName().CStr(),
				  chunk_stream->GetAppId(), chunk_stream->GetStreamId(),
				  remote->ToString().CStr());

			_physical_port->DisconnectClient(chunk_stream->GetRemoteSocket());
		}
	}
}

void RtmpServer::OnDisconnected(const std::shared_ptr<ov::Socket> &remote, PhysicalPortDisconnectReason reason, const std::shared_ptr<const ov::Error> &error)
{
	std::unique_lock<std::recursive_mutex> lock(_chunk_context_list_mutex);

	auto item = _chunk_context_list.find(remote.get());

	if (item != _chunk_context_list.end())
	{
		auto &chunk_stream = item->second;

		// logte("chunk_stream->GetAppId() : %u, chunk_stream->GetStreamId() : %u", chunk_stream->GetAppId(), chunk_stream->GetStreamId());
		// Stream Delete
		if (chunk_stream->GetAppId() != info::InvalidApplicationId && chunk_stream->GetStreamId() != info::InvalidStreamId)
		{
			if (reason == PhysicalPortDisconnectReason::Disconnect)
			{
				logti("The RTMP client is disconnected: [%s/%s] (%u/%u), remote: %s",
					chunk_stream->GetAppName().CStr(), chunk_stream->GetStreamName().CStr(),
					chunk_stream->GetAppId(), chunk_stream->GetStreamId(),
					remote->ToString().CStr());
			}
			else
			{
				logti("The RTMP client is disconnected: [%s/%s] (%u/%u), remote: %s",
					chunk_stream->GetAppName().CStr(), chunk_stream->GetStreamName().CStr(),
					chunk_stream->GetAppId(), chunk_stream->GetStreamId(),
					remote->ToString().CStr());
			}

			OnDeleteStream(chunk_stream->GetRemoteSocket(),
						   chunk_stream->GetAppName(), chunk_stream->GetStreamName(),
						   chunk_stream->GetAppId(), chunk_stream->GetStreamId());
		}

		_chunk_context_list.erase(item);
	}
}

bool RtmpServer::OnChunkStreamReady(ov::ClientSocket *remote,
									ov::String &app_name, ov::String &stream_name,
									std::shared_ptr<RtmpMediaInfo> &media_info,
									info::application_id_t &application_id,
									uint32_t &stream_id)
{
	logti("[%s/%s] RTMP Provider stream has been created: id(%u/%u) device(%s) remote(%s)",
		  app_name.CStr(), stream_name.CStr(),
		  application_id, stream_id,
		  RtmpChunkStream::GetEncoderTypeString(media_info->encoder_type).CStr(),
		  remote->ToString().CStr());

	// Notify the ready stream event to the observers
	std::shared_lock<std::shared_mutex> lock(_observers_lock);
	for (auto &observer : _observers)
	{
		if (observer->OnStreamReadyComplete(app_name, stream_name, media_info, application_id, stream_id) == false)
		{
			logtd("An error occurred while notify RTMP ready event to observers for [%s/%s] remote(%s)",
				  app_name.CStr(), stream_name.CStr(),
				  remote->ToString().CStr());
			return false;
		}
	}

	return true;
}

bool RtmpServer::OnChunkStreamVideoData(ov::ClientSocket *remote,
										info::application_id_t application_id, uint32_t stream_id,
										uint64_t timestamp,
										RtmpFrameType frame_type,
										const std::shared_ptr<const ov::Data> &data)
{
	// Notify the video data to the observers
	std::shared_lock<std::shared_mutex> lock(_observers_lock);
	for (auto &observer : _observers)
	{
		if (observer->OnVideoData(application_id, stream_id, timestamp, frame_type, data) == false)
		{
			logte("Could not send video data to observer %p: (%u/%u), remote: %s",
				  observer.get(),
				  application_id, stream_id,
				  remote->ToString().CStr());
			return false;
		}
	}

	return true;
}

bool RtmpServer::OnChunkStreamAudioData(ov::ClientSocket *remote,
										info::application_id_t application_id, uint32_t stream_id,
										uint64_t timestamp,
										RtmpFrameType frame_type,
										const std::shared_ptr<const ov::Data> &data)
{
	// Notify the audio data to the observers
	std::shared_lock<std::shared_mutex> lock(_observers_lock);
	for (auto &observer : _observers)
	{
		if (!observer->OnAudioData(application_id, stream_id, timestamp, frame_type, data))
		{
			logte("Could not send audio data to observer %p: (%u/%u), remote: %s",
				  observer.get(),
				  application_id, stream_id,
				  remote->ToString().CStr());
			return false;
		}
	}

	return true;
}

bool RtmpServer::OnDeleteStream(ov::ClientSocket *remote,
								ov::String &app_name, ov::String &stream_name,
								info::application_id_t application_id, uint32_t stream_id)
{
	// Notify the delete stream event to the observers
	std::shared_lock<std::shared_mutex> lock(_observers_lock);
	for (auto &observer : _observers)
	{
		if (observer->OnDeleteStream(application_id, stream_id) == false)
		{
			logtd("Could not delete the RTMP stream from observer: [%s/%s] (%u/%u), remote: %s",
				  app_name.CStr(), stream_name.CStr(),
				  application_id, stream_id,
				  remote->ToString().CStr());

			return false;
		}
	}

	logti("[%s/%s] RTMP Provider stream has been deleted (%u/%u), remote: %s",
		  app_name.CStr(), stream_name.CStr(),
		  application_id, stream_id,
		  remote->ToString().CStr());

	return true;
}

#define MAX_STREAM_PACKET_GAP (10)

ov::DelayQueueAction RtmpServer::OnGarbageCheck(void *parameter)
{
	time_t current_time = ::time(nullptr);
	std::map<ov::Socket *, std::shared_ptr<RtmpChunkStream>> garbage_list;

	{
		std::unique_lock<std::recursive_mutex> lock(_chunk_context_list_mutex);

		for (auto &item : _chunk_context_list)
		{
			auto &chunk_stream = item.second;
			auto elapsed = current_time - chunk_stream->GetLastPacketTime();

			if (elapsed > MAX_STREAM_PACKET_GAP)
			{
				logtw("RTMP input stream has timed out: [%s/%s] (%u/%u), elapsed: %d, threshold: %d",
					  chunk_stream->GetAppName().CStr(), chunk_stream->GetStreamName().CStr(),
					  chunk_stream->GetAppId(), chunk_stream->GetStreamId(),
					  elapsed, MAX_STREAM_PACKET_GAP);

				garbage_list.emplace(item);
			}
		}
	}

	for (auto &garbage : garbage_list)
	{
		auto &chunk_stream = garbage.second;

		if (chunk_stream->GetRemoteSocket() != nullptr)
		{
			logtw("RTMP stream %s is timed out. Disconnecting...", chunk_stream->GetRemoteSocket()->ToString().CStr());
		}
		else
		{
			OV_ASSERT2(chunk_stream->GetRemoteSocket() != nullptr);
			logtw("RTMP stream (Unknown) is timed out. Disconnecting...");
		}

		// Close the socket
		_physical_port->DisconnectClient(dynamic_cast<ov::ClientSocket *>(garbage.first));
	}

	return ov::DelayQueueAction::Repeat;
}
