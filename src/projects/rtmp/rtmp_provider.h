//==============================================================================
//
//  RtmpProvider
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include <stdint.h>
#include <memory>
#include <vector>
#include <algorithm>
#include <thread>

#include <base/ovlibrary/ovlibrary.h>

#include "base/application/application.h"

#include "base/provider/provider.h"
#include "base/provider/application.h"

#include "proto/rtmp_tcp_server.h"

class RtmpProvider : public pvd::Provider, public RtmpTcpServer
{
	// class TranscodeApplication;
public:
	static std::shared_ptr<RtmpProvider> Create(std::shared_ptr<MediaRouteInterface> router);

    RtmpProvider(std::shared_ptr<MediaRouteInterface> router);
    ~RtmpProvider();

    // 프로파이더의 이름을 구분함
	ov::String 	GetProviderName() override
	{
		return "RTMP";
	}

	bool Start() override ;
	bool Stop() override ;

	std::shared_ptr<pvd::Application> OnCreateApplication(ApplicationInfo &info);

	//============================================================================================================
	// Rtmp Tcp Server 인터페이스
	//============================================================================================================
	int OnConnect(RtmpConnector *conn, ov::String app, ov::String flashVer, ov::String swfUrl, ov::String tcUrl) override;
	void OnReleaseStream(RtmpConnector *conn, ov::String stream) override;
	void OnFCPublish(RtmpConnector *conn, ov::String stream) override;
	void OnCreateStream(RtmpConnector *conn) override;
	void OnPublish(RtmpConnector *conn, ov::String stream, ov::String type) override;
	void OnMetaData(RtmpConnector *conn) override;
	void OnFCUnpublish(RtmpConnector *conn, ov::String stream) override;
	void OnDeleteStream(RtmpConnector *conn) override;
	void OnVideoPacket(RtmpConnector *conn, int8_t has_abs_time, uint32_t time, int8_t* body, uint32_t body_size) override;
	void OnAudioPacket(RtmpConnector *conn, int8_t has_abs_time, uint32_t time, int8_t* body, uint32_t body_size) override;
	void OnDisconnect(RtmpConnector *conn) override;
};

