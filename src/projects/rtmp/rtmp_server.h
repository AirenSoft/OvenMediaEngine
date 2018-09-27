//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once
#include "rtmp_chunk_stream.h"
#include <base/ovsocket/ovsocket.h>
#include <physical_port/physical_port_manager.h>
#include "rtmp_observer.h"

// TODO : oberver 방식으로 변경
//====================================================================================================
// RtmpServer
//====================================================================================================
class RtmpServer : protected PhysicalPortObserver, public IRtmpChunkStream
{
public:
	RtmpServer();
	virtual ~RtmpServer();

public:
	bool Start(const ov::SocketAddress &address);
	bool Stop();

	bool AddObserver(const std::shared_ptr<RtmpObserver> &observer);
	bool RemoveObserver(const std::shared_ptr<RtmpObserver> &observer);

protected:
	//--------------------------------------------------------------------
	// Implementation of PhysicalPortObserver
	//--------------------------------------------------------------------
	void OnConnected(ov::Socket *remote) override;
	void OnDataReceived(ov::Socket *remote, const ov::SocketAddress &address, const std::shared_ptr<const ov::Data> &data) override;
	void OnDisconnected(ov::Socket *remote, PhysicalPortDisconnectReason reason, const std::shared_ptr<const ov::Error> &error) override;

	//--------------------------------------------------------------------
	// Implementation of IRtmpChunkStream
	//--------------------------------------------------------------------
	bool OnChunkStreamReadyComplete(ov::ClientSocket *remote, ov::String &app_name, ov::String &stream_name, std::shared_ptr<RtmpMediaInfo> &media_info, uint32_t &app_id, uint32_t &stream_id) override;
	bool OnChunkStreamVideoData(ov::ClientSocket *remote, uint32_t app_id, uint32_t stream_id, uint32_t timestamp, std::shared_ptr<std::vector<uint8_t>> &data)  override;
	bool OnChunkStreamAudioData(ov::ClientSocket *remote, uint32_t app_id, uint32_t stream_id, uint32_t timestamp, std::shared_ptr<std::vector<uint8_t>> &data) override;
	bool OnChunkStreamDelete(ov::ClientSocket *remote, ov::String &app_name, ov::String &stream_name, uint32_t app_id, uint32_t stream_id) override;

private :
	std::shared_ptr<PhysicalPort>                               _physical_port;
	std::map<ov::Socket *, std::shared_ptr<RtmpChunkStream>>    _client_list;
	std::vector<std::shared_ptr<RtmpObserver>>                  _observers;

};