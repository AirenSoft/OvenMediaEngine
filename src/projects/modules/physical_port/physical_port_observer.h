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
#include <base/ovsocket/ovsocket.h>

#include <memory>

class PhysicalPort;

enum class PhysicalPortDisconnectReason
{
	/// An error occurred
	Error,
	/// Disconnected by server
	Disconnect,
	/// Disconnected from client
	Disconnected,
};

enum class PhysicalPortProbeResult
{
	Failed,
	Success,
	NeedMoreData,
};

class PhysicalPortObserver
{
public:
	friend class PhysicalPort;

	virtual ~PhysicalPortObserver() = default;

	// Called when the client is connected
	virtual void OnConnected(const std::shared_ptr<ov::Socket> &remote)
	{
		// dummy function
	}

	// Called when the packet is received
	virtual void OnDataReceived(const std::shared_ptr<ov::Socket> &remote, const ov::SocketAddress &address, const std::shared_ptr<const ov::Data> &data) = 0;

	// Called when the client is disconnected
	virtual void OnDisconnected(const std::shared_ptr<ov::Socket> &remote, PhysicalPortDisconnectReason reason, const std::shared_ptr<const ov::Error> &error)
	{
		// dummy function
	}

	// When data is received, the PhysicalPort asks if it is your data
	virtual PhysicalPortProbeResult ProbePacket(const std::shared_ptr<ov::Socket> &remote, const std::shared_ptr<const ov::Data> &data)
	{
		// dummy function
		return PhysicalPortProbeResult::Failed;
	}
};
