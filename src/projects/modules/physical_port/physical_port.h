//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/ovsocket/ovsocket.h>

#include <functional>
#include <memory>
#include <thread>

#include "physical_port_observer.h"

class PhysicalPortWorker;

class PhysicalPort : public ov::EnableSharedFromThis<PhysicalPort>
{
public:
	friend class PhysicalPortWorker;

	PhysicalPort() = default;
	virtual ~PhysicalPort();

	bool Create(ov::SocketType type,
				const ov::SocketAddress &address,
				int send_buffer_size,
				int recv_buffer_size,
				int worker_count,
				int socket_pool_worker_count);

	bool Close();

	void IncreaseRefCount()
	{
		_ref_count++;
	}

	void DecreaseRefCount()
	{
		_ref_count--;
	}

	int GetRefCount() const
	{
		return _ref_count;
	}

	ov::SocketState GetState() const;

	ov::SocketType GetType() const
	{
		return _type;
	}

	const ov::SocketAddress &GetAddress() const
	{
		return _address;
	}

	std::shared_ptr<const ov::Socket> GetSocket() const;
	std::shared_ptr<ov::Socket> GetSocket();

	size_t GetWorkerCount() const
	{
		return _worker_list.size();
	}

	size_t GetSocketPoolWorkerCount() const
	{
		return _socket_pool->GetWorkerCount();
	}

	bool AddObserver(PhysicalPortObserver *observer);

	bool RemoveObserver(PhysicalPortObserver *observer);

	bool DisconnectClient(ov::ClientSocket *client_socket);

	ov::String ToString() const;

protected:
	bool CreateServerSocket(ov::SocketType type,
							const ov::SocketAddress &address,
							int send_buffer_size,
							int recv_buffer_size,
							int worker_count,
							int socket_pool_worker_count);

	bool CreateDatagramSocket(ov::SocketType type,
							  const ov::SocketAddress &address,
							  int worker_count,
							  int socket_pool_worker_count);

	// For TCP physical port
	ov::SocketConnectionState OnClientConnectionStateChanged(const std::shared_ptr<ov::ClientSocket> &client, ov::SocketConnectionState state, const std::shared_ptr<ov::Error> &error);
	ov::SocketConnectionState OnClientData(const std::shared_ptr<ov::ClientSocket> &client, const std::shared_ptr<const ov::Data> &data);

	// For UDP physical port
	void OnDatagram(const std::shared_ptr<ov::DatagramSocket> &client, const ov::SocketAddress &remote_address, const std::shared_ptr<ov::Data> &data);

	std::shared_ptr<ov::SocketPool> _socket_pool;

	ov::SocketType _type = ov::SocketType::Unknown;
	ov::SocketAddress _address;

	std::shared_ptr<ov::ServerSocket> _server_socket;
	std::shared_ptr<ov::DatagramSocket> _datagram_socket;

	std::atomic<int> _ref_count{0};

	// TODO(dimiden): Must use a mutex to prevent race condition
	std::vector<PhysicalPortObserver *> _observer_list;

	std::shared_mutex _worker_mutex;
	std::vector<std::shared_ptr<PhysicalPortWorker>> _worker_list;
};
