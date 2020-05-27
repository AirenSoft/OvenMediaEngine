//==============================================================================
//
//  PushProvider Base Class 
//
//  Created by Getroot
//  Copyright (c) 2020 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include <base/provider/provider.h>

#include "application.h"
#include "stream.h"

namespace pvd
{
    class PushProvider : public Provider
    {
    public:
		// Implementation Provider -> OrchestratorModuleInterface
		OrchestratorModuleType GetModuleType() const override
		{
			return OrchestratorModuleType::PushProvider;
		}

		// Call CreateServer and store the server 
		virtual bool Start() override;
		virtual bool Stop() override;

		// stream has to have applicaiton/stream and track informaiton
		virtual bool AssignStream(ov::String app_name, std::shared_ptr<PushStream> &stream);

    protected:
		PushProvider(const cfg::Server &server_config, const std::shared_ptr<MediaRouteInterface> &router);
		virtual ~PushProvider();

		// CreateStream 후 _streams에 stream 보관, app/stream이 지정되어 있으면 AssignStream 수행
		// 비록 UDP라도 첫번째 패킷에서 OnConnected를 호출하여 StreamMold를 생성해야 한다. 
		bool OnConnected(uint32_t client_id);

		// Data를 수신 받으면 stream에 넣는다. 
		bool OnDataReceived(uint32_t client_id, const std::shared_ptr<const ov::Data> &data);

		// _stream에서 제거하고, Assign 되어 있다면 provider->OnStreamDeleted
		// UDP Timeout은 자식이 처리해야 한다. 
		bool OnDisconnected(uint32_t client_id, const std::shared_ptr<const ov::Error> &error);

    private:
		// Interleaved stream의 경우, 아직 정확한 정보를 모르는 경우 StreamMold를 생성해서 정보가 모두 모아지면 스스로를 AssignStream에 넣는다.  
		// Signalling, Data channel이 분리된 경우에는 StreamMold에서 Signalling을 처리하고, Data를 처리하는 Stream을 생성해서 Assign 한다. 
		// Signalling이 없는 경우 설정된대로 stream을 생성하여 AssignStream에 바로 넣을 수도 있지만, 초기 패킷을 받아서 Track을 분석해야 하는 경우 StreamMold -> AssignStream을 수행할수도 있다. 
		virtual std::shared_ptr<pvd::PushStream> CreateStreamMold() = 0;

		// All stream molds
		std::map<uint32_t, std::shared_ptr<PushStream>>	_stream_molds;
    };
}