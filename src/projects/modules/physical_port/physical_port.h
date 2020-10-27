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

// PhysicalPort는 여러 곳에서 공유해서 사용할 수 있음
// PhysicalPortObserver를 iteration 하면서 callback 할 수 있는 구조 필요
class PhysicalPort : public ov::EnableSharedFromThis<PhysicalPort>
{
public:
	friend class PhysicalPortWorker;

	PhysicalPort();
	virtual ~PhysicalPort();

	bool Create(ov::SocketType type,
				const ov::SocketAddress &address,
				int send_buffer_size = 0,
				int recv_buffer_size = 0);

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

	bool AddObserver(PhysicalPortObserver *observer);

	bool RemoveObserver(PhysicalPortObserver *observer);

	bool DisconnectClient(ov::ClientSocket *client_socket);

	ov::String ToString() const;

protected:
	bool CreateServerSocket(ov::SocketType type,
							const ov::SocketAddress &address,
							int send_buffer_size,
							int recv_buffer_size);

	bool CreateDatagramSocket(ov::SocketType type, const ov::SocketAddress &address);

	ov::SocketType _type;
	ov::SocketAddress _address;

	std::shared_ptr<ov::ServerSocket> _server_socket;
	std::shared_ptr<ov::DatagramSocket> _datagram_socket;

	volatile bool _need_to_stop;
	std::thread _thread;

	std::atomic<int> _ref_count { 0 };

	// TODO(dimiden): Must use a mutex to prevent race condition
	std::vector<PhysicalPortObserver *> _observer_list;

	std::shared_mutex _worker_mutex;
	std::vector<std::shared_ptr<PhysicalPortWorker>> _worker_list;
};
