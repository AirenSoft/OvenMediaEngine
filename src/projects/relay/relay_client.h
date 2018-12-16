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

#include <utility>

#include <base/ovlibrary/ovlibrary.h>
#include <base/ovsocket/socket.h>
#include <base/application/application.h>
#include <base/media_route/media_buffer.h>
#include <base/media_route/media_route_application_connector.h>
#include <media_router/media_route_application.h>

class RelayClient : public MediaRouteApplicationConnector
{
public:
	RelayClient(MediaRouteApplication *media_route_application, const info::Application &application_info, ov::String server_address)
		: _media_route_application(media_route_application),
		  _application_info(application_info),
		  _server_address(std::move(server_address))
	{
	}

	void Start();
	void Stop();

	void SendPacket(const RelayPacket &packet);
	void SendPacket(RelayPacketType type, info::application_id_t application_id, info::stream_id_t stream_id, const MediaPacket &packet);

	// identifier = <app>/<stream>
	void Sync(const ov::String &identifier);

	//--------------------------------------------------------------------
	// Implementation of MediaRouteApplicationConnector
	//--------------------------------------------------------------------
	ConnectorType GetConnectorType() override
	{
		return ConnectorType::Relay;
	}
	//--------------------------------------------------------------------

protected:
	struct Transaction
	{
		uint32_t transaction_id = 0xFFFFFFFF;
		int8_t media_type;
		uint64_t last_pts;
		uint32_t track_id;
		ov::Data data;
		uint8_t flags;
	};

	void HandleSync(const RelayPacket &packet);
	void HandleData(const RelayPacket &packet);

	MediaRouteApplication *_media_route_application;

	const info::Application &_application_info;

	ov::String _server_address;
	ov::Socket _client_socket;

	std::thread _connection;
	bool _stop = true;

	// transactions are used to reassemble packets
	std::mutex _transaction_lock;
	// key: stream_id, value: [key: track_id, value: transaction data]
	std::map<info::stream_id_t, std::map<uint32_t, std::shared_ptr<Transaction>>> _transactions;
};