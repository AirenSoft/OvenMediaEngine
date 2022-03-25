//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/ovlibrary/ovlibrary.h>
#include <modules/physical_port/physical_port_observer.h>

#include <functional>

#include "../../http_datastructure.h"

namespace http
{
	namespace svr
	{
		namespace ws
		{
			class Client;
			class Frame;

			// WebSocket.on('open')
			typedef std::function<void(std::shared_ptr<Client> &client)> ConnectCallback;
			// WebSocket.on('message')
			typedef std::function<void(std::shared_ptr<Client> &client, const std::shared_ptr<const ov::Data> &data)> MessageCallback;
			// WebSocket.on('error')
			typedef std::function<void(std::shared_ptr<Client> &client, const std::shared_ptr<const ov::Error> &error)> ErrorCallback;

			typedef std::function<bool(const std::shared_ptr<Client> &ws_client)> ConnectionHandler;
			typedef std::function<bool(const std::shared_ptr<Client> &ws_client, const std::shared_ptr<const ov::Data> &message)> MessageHandler;
			typedef std::function<void(const std::shared_ptr<Client> &ws_client, const std::shared_ptr<const ov::Error> &error)> ErrorHandler;
			typedef std::function<void(const std::shared_ptr<Client> &ws_client, PhysicalPortDisconnectReason reason)> CloseHandler;
		}  // namespace ws
	} // namespace svr
}  // namespace http
