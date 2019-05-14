//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "relay_datastructure.h"

#include <base/ovsocket/socket.h>
#include <base/application/application.h>
#include <physical_port/physical_port_manager.h>
#include <base/media_route/media_route_application_interface.h>
#include <base/media_route/media_route_application_observer.h>

class MediaRouteStream;

class RelayServer : public PhysicalPortObserver, public MediaRouteApplicationObserver
{
public:
	RelayServer(MediaRouteApplicationInterface *media_route_application, const info::Application *application_info);
	~RelayServer() override;

	void Send(info::stream_id_t stream_id, const RelayPacket &base_packet, const ov::Data *data);
	void Send(info::stream_id_t stream_id, const RelayPacket &base_packet, const void *data, uint16_t data_size);
	void Send(const std::shared_ptr<ov::Socket> &remote, info::stream_id_t stream_id, const RelayPacket &base_packet, const ov::Data *data);
	void SendMediaPacket(const std::shared_ptr<MediaRouteStream> &media_stream, const MediaPacket *packet);

protected:
	struct ClientInfo
	{
	};

	void SendStream(const std::shared_ptr<ov::Socket> &remote, const std::shared_ptr<StreamInfo> &stream_info);

	//--------------------------------------------------------------------
	// Implementation of MediaRouteApplicationObserver
	//--------------------------------------------------------------------
	bool OnCreateStream(std::shared_ptr<StreamInfo> info) override;
	bool OnDeleteStream(std::shared_ptr<StreamInfo> info) override;
	bool OnSendVideoFrame(std::shared_ptr<StreamInfo> stream, std::shared_ptr<MediaTrack> track, std::unique_ptr<EncodedFrame> encoded_frame, std::unique_ptr<CodecSpecificInfo> codec_info, std::unique_ptr<FragmentationHeader> fragmentation) override;
	bool OnSendAudioFrame(std::shared_ptr<StreamInfo> stream, std::shared_ptr<MediaTrack> track, std::unique_ptr<EncodedFrame> encoded_frame, std::unique_ptr<CodecSpecificInfo> codec_info, std::unique_ptr<FragmentationHeader> fragmentation) override;

	ObserverType GetObserverType() override
	{
		return ObserverType::Relay;
	}
	//--------------------------------------------------------------------


	//--------------------------------------------------------------------
	// Implementation of PhysicalPortObserver
	//--------------------------------------------------------------------
	void OnConnected(const std::shared_ptr<ov::Socket> &remote) override;
	void OnDataReceived(const std::shared_ptr<ov::Socket> &remote, const ov::SocketAddress &address, const std::shared_ptr<const ov::Data> &data) override;
	void OnDisconnected(const std::shared_ptr<ov::Socket> &remote, PhysicalPortDisconnectReason reason, const std::shared_ptr<const ov::Error> &error) override;
	//--------------------------------------------------------------------

	void HandleRegister(const std::shared_ptr<ov::Socket> &remote, const RelayPacket &packet);

	MediaRouteApplicationInterface *_media_route_application;

	const info::Application *_application_info;
	std::shared_ptr<PhysicalPort> _server_port;

	// All client list
	std::mutex _client_list_mutex;
	std::map<ov::Socket *, ClientInfo> _client_list;

	uint32_t _transaction_id = 0;
};