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

#define RELAY_DEFAULT_PORT                              9000

class RelayClient : public MediaRouteApplicationConnector
{
public:
	RelayClient(MediaRouteApplication *media_route_application, const info::Application *application_info)
		: _media_route_application(media_route_application),
		  _application_info(application_info)
	{
	}

	void Start(const ov::String &application);
	void Stop();

	void SendPacket(const RelayPacket &packet);
	void SendPacket(RelayPacketType type, info::application_id_t application_id, info::stream_id_t stream_id, const MediaPacket &packet);

	void Register(const ov::String &identifier);

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

	struct RelayStreamInfo
	{
		std::shared_ptr<StreamInfo> stream_info;

		// [key: track_id, value: transaction data]
		std::map<uint32_t, std::shared_ptr<Transaction>> transactions;
	};

	ov::String ParseAddress(const ov::String &address);

	std::shared_ptr<RelayStreamInfo> GetStreamInfo(info::stream_id_t stream_id, bool create_info = false, bool *created = nullptr, bool delete_info = false);

	void HandleCreateStream(const RelayPacket &packet);
	void HandleDeleteStream(const RelayPacket &packet);
	void HandleData(const RelayPacket &packet);

	MediaRouteApplication *_media_route_application;

	const info::Application *_application_info;

	ov::Socket _client_socket;

	std::thread _connection;
	bool _stop = true;

	std::mutex _stream_list_mutex;
	std::map<info::stream_id_t, std::shared_ptr<RelayStreamInfo>> _stream_list;
};