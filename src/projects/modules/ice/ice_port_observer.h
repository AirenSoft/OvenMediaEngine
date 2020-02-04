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
#include <base/info/session.h>
#include <modules/sdp/session_description.h>

enum class IcePortConnectionState : int
{
	// 새로운 클라이언트가 접속함
		New,
	// <Browser의 Binding request> ~ <Browser의 Binding response> 까지의 상태
		Checking,
	// Binding request & response가 올바르게 잘 된 상태
		Connected,
	Completed,
	// 오류 발생
		Failed,
	// 접속이 해제된 상태. Timeout이 발생한 경우에도 disconnected이 전달됨
		Disconnected,
	Closed,

	// 개수 세기 위한 용도
		Max,
};

class IcePort;

class IcePortObserver : public ov::EnableSharedFromThis<IcePortObserver>
{
public:
	// ICE 접속 상태가 바뀜
	virtual void OnStateChanged(IcePort &port, const std::shared_ptr<info::Session> &session, IcePortConnectionState state)
	{
		// dummy function
	}

	// ICE 경로가 바뀜. 즉, A candidate로 통신하다 실패하면 B candidate로 절체되는데, 이 때 호출됨.
	// 일반적인 경우 이 이벤트를 처리할 필요 없음
	// TODO: OnRouteChanged 이벤트가 notify 되도록 구현
	virtual void OnRouteChanged(IcePort &port)
	{
		// dummy function
	}

	// 데이터가 수신됨
	virtual void OnDataReceived(IcePort &port, const std::shared_ptr<info::Session> &session, std::shared_ptr<const ov::Data> data) = 0;
};