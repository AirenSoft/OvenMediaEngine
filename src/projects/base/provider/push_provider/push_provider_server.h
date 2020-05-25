//==============================================================================
//
//  PushProviderServer
//
//  Created by Getroot
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include "push_provider.h"

namespace pvd
{
	class PushProviderServer
	{
	public:
		// Start server
		virtual bool CreateServer(const ov::SocketAddress &address) = 0;
		bool CreateServer(const ov::SocketAddress &address, ov::String app_name, ov::String stream_name);
		// Stop Server
		virtual bool StopServer(const ov::SocketAddress &address) = 0;

	protected:
		PushProviderServer(const std::shared_ptr<PushProvider> &provider);
		~PushProviderServer();

		// 접속이 들어왔을 때 Stream을 생성한다. 자식 Server는 그에 맞는 Stream을 생성하여 제공해야 한다. 
		virtual std::shared_ptr<pvd::Stream> CreateStream(const std::shared_ptr<info::Stream> &stream_info) = 0;

		// CreateStream 후 _streams에 stream 보관, app/stream이 지정되어 있으면 AssignStream 수행
		void OnConnected(uint32_t client_id);

		// Data를 수신 받으면 stream에 넣는다. 
		// UDP의 경우는 DispatchConnect 없이 이 함수가 바로 호출되므로 관련 처리를 해야 한다. 
		void OnDataReceived(uint32_t client_id, const std::shared_ptr<const ov::Data> &data);

		// _stream에서 제거하고, Assign 되어 있다면 provider->OnStreamDeleted
		void OnDisconnected(uint32_t client_id, const std::shared_ptr<const ov::Error> &error);

	private:
		std::map<uint32_t, std::shared_ptr<pvd::Stream>>	_streams;
	};
}