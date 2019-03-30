//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <memory>

#include <base/ovlibrary/ovlibrary.h>
#include <base/ovsocket/ovsocket.h>

class PhysicalPort;

enum class PhysicalPortDisconnectReason
{
	Error,
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

	// TCP/SRT등 일 때, 상대방이 접속하면 호출됨
	virtual void OnConnected(const std::shared_ptr<ov::Socket> &remote)
	{
		// dummy function
	}

	// 데이터를 수신하였을 때 호출됨
	virtual void OnDataReceived(const std::shared_ptr<ov::Socket> &remote, const ov::SocketAddress &address, const std::shared_ptr<const ov::Data> &data) = 0;

	// TCP/SRT등 일 때, 상대방과의 접속이 해제되면 호출됨
	virtual void OnDisconnected(const std::shared_ptr<ov::Socket> &remote, PhysicalPortDisconnectReason reason, const std::shared_ptr<const ov::Error> &error)
	{
		// dummy function
	}

	// 데이터가 수신되었을 때, 자신이 처리할 수 있는 패킷인지 확인
	virtual PhysicalPortProbeResult ProbePacket(const std::shared_ptr<ov::Socket> &remote, const std::shared_ptr<const ov::Data> &data)
	{
		// dummy function
		return PhysicalPortProbeResult::Failed;
	}
};
