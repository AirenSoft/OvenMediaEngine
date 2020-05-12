//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "rtmp_chunk_stream.h"
#include "rtmp_observer.h"

#include <map>

#include <base/ovsocket/ovsocket.h>
#include <modules/physical_port/physical_port_manager.h>

class RtmpServer : protected PhysicalPortObserver, public IRtmpChunkStream
{
public:
    RtmpServer() = default;
    virtual ~RtmpServer();

    bool Start(const ov::SocketAddress &address);
    bool Stop();
    bool AddObserver(const std::shared_ptr<RtmpObserver> &observer);
    bool RemoveObserver(const std::shared_ptr<RtmpObserver> &observer);

	bool Disconnect(const ov::String &app_name);
    bool Disconnect(const ov::String &app_name, uint32_t stream_id);

protected:
	//--------------------------------------------------------------------
	// Implementation of PhysicalPortObserver
	//--------------------------------------------------------------------
	void OnConnected(const std::shared_ptr<ov::Socket> &remote) override;

	void OnDataReceived(const std::shared_ptr<ov::Socket> &remote,
						const ov::SocketAddress &address,
						const std::shared_ptr<const ov::Data> &data) override;

	void OnDisconnected(const std::shared_ptr<ov::Socket> &remote,
						PhysicalPortDisconnectReason reason,
						const std::shared_ptr<const ov::Error> &error) override;

	//--------------------------------------------------------------------
	// Implementation of IRtmpChunkStream
	//--------------------------------------------------------------------
	bool OnChunkStreamReady(ov::ClientSocket *remote,
							ov::String &app_name, ov::String &stream_name,
							std::shared_ptr<RtmpMediaInfo> &media_info,
							info::application_id_t &application_id,
							uint32_t &stream_id) override;

	bool OnChunkStreamVideoData(ov::ClientSocket *remote,
								info::application_id_t application_id, uint32_t stream_id,
								uint64_t timestamp,
								RtmpFrameType frame_type,
								const std::shared_ptr<const ov::Data> &data) override;

	bool OnChunkStreamAudioData(ov::ClientSocket *remote,
								info::application_id_t application_id, uint32_t stream_id,
								uint64_t timestamp,
								RtmpFrameType frame_type,
								const std::shared_ptr<const ov::Data> &data) override;

	bool OnDeleteStream(ov::ClientSocket *remote,
						ov::String &app_name, ov::String &stream_name,
						info::application_id_t application_id, uint32_t stream_id) override;

	ov::DelayQueueAction OnGarbageCheck(void *parameter);

private:
	std::shared_ptr<PhysicalPort> _physical_port;
	std::vector<std::shared_ptr<RtmpObserver>> _observers;
	std::shared_mutex _observers_lock;

	std::recursive_mutex _chunk_context_list_mutex;
	std::map<ov::Socket *, std::shared_ptr<RtmpChunkStream>> _chunk_context_list;
	ov::DelayQueue _garbage_check_timer;
	ov::StopWatch _stat_stop_watch;
};
