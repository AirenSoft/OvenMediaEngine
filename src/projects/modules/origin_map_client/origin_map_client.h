//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2022 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/common_types.h>
#include <base/ovlibrary/ovlibrary.h>
#include <base/ovlibrary/delay_queue.h>
#include <hiredis/hiredis.h>

// If Origins-Edges cluster uses OriginMapStore, app/stream must be unique in the cluster.
class OriginMapClient
{
public:
	// redis_host: redis server host (ex: 192.168.0.160:6379)
	// redis_password: redis server password (ex: password!@#)
	OriginMapClient(const ov::String &redis_host, const ov::String &redis_password);

	// if return false, it means that the app_stream_name is already registered from other origin server
	// app_stream_name : app/stream name (ex: app/stream)
	// origin_host : OVT provided origin host info (ex: 192.168.0.160:9000)
	bool Register(const ov::String &app_stream_name, const ov::String &origin_host);
	bool Update(const ov::String &app_stream_name, const ov::String &origin_host);
	bool Unregister(const ov::String &app_stream_name);

	CommonErrorCode GetOrigin(const ov::String &app_stream_name, ov::String &origin_host);

private:
	bool CheckConnection();
	bool ConnectRedis();

	bool NofifyStreamsAlive();

	ov::String _redis_ip;
	uint16_t _redis_port;
	ov::String _redis_password;

	ov::DelayQueue _update_timer{"OMapC"};

	std::map<ov::String, ov::String> _origin_map;
	std::mutex _origin_map_mutex;

	redisContext *_redis_context = nullptr;
	std::mutex _redis_context_mutex;
};