//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2022 AirenSoft. All rights reserved.
//
//==============================================================================

#include "origin_map_client.h"

#define OV_LOG_TAG "OriginMapClient"

OriginMapClient::OriginMapClient(const ov::String &redis_host, const ov::String &redis_password)
{
	// Parse ip:port 
	auto ip_port = redis_host.Split(":");
	if (ip_port.size() != 2)
	{
		return;
	}

	_redis_ip = ip_port[0];
	_redis_port = ov::Converter::ToUInt16(ip_port[1]);
	_redis_password = redis_password;

	_update_timer.Push(
		[this](void *paramter) -> ov::DelayQueueAction {
			RetryRegister();
			NofifyStreamsAlive();
			return ov::DelayQueueAction::Repeat;
		},
		2500);
	_update_timer.Start();
}

bool OriginMapClient::NofifyStreamsAlive()
{
	std::unique_lock<std::mutex> lock(_origin_map_mutex);
	auto origin_map = _origin_map;
	lock.unlock();

	for (auto &[key, value] : origin_map)
	{
		Update(key, value);
	}

	return true;
}

bool OriginMapClient::RetryRegister()
{
	std::unique_lock<std::mutex> lock(_origin_map_mutex);
	if (_origin_map_candidates.size() == 0)
	{
		return true;
	}

	auto origin_map_candidates = _origin_map_candidates;
	lock.unlock();

	std::vector<ov::String> keys_to_remove;
	for (auto &[key, value] : origin_map_candidates)
	{
		if (Register(key, value) == true)
		{
			keys_to_remove.push_back(key);
		}
	}

	if (keys_to_remove.size() > 0)
	{
		std::lock_guard<std::mutex> lock(_origin_map_mutex);
		for (auto &key : keys_to_remove)
		{
			_origin_map_candidates.erase(key);
		}
	}

	return true;
}

bool OriginMapClient::Register(const ov::String &app_stream_name, const ov::String &origin_host)
{
	if (ConnectRedis() == false)
	{
		logte("Failed to connect redis server : %s:%d (err:%s)", _redis_ip.CStr(), _redis_port, _redis_context!=nullptr?_redis_context->errstr:"nil");
		return false;
	}

	std::unique_lock<std::mutex> lock(_redis_context_mutex);

	bool is_already_registered = false;

	// Check if the app/stream is already registered with same origin host
	redisReply *reply = (redisReply *)redisCommand(_redis_context, "GET %s", app_stream_name.CStr());
	if (reply == nullptr || reply->type == REDIS_REPLY_ERROR)
	{
		logte("Failed to get origin host from redis : %s:%d (err:%s)", _redis_ip.CStr(), _redis_port, reply!=nullptr?reply->str:"nil");
		return false;
	}
	else if (reply->type == REDIS_REPLY_NIL)
	{
		// Not exist, keep going
	}
	else if (reply->type == REDIS_REPLY_STRING)
	{
		if (origin_host == reply->str)
		{
			is_already_registered = true;
		}
		else
		{
			logte("<%s> stream is already registered with different origin host (%s)", app_stream_name.CStr(), reply->str);
			freeReplyObject(reply);
			lock.unlock();

			std::lock_guard<std::mutex> origin_map_lock(_origin_map_mutex);
			_origin_map_candidates[app_stream_name] = origin_host;

			return false;
		}
	}
	freeReplyObject(reply);

	if (is_already_registered == false)
	{
		// Set origin host to redis
		// The EXPIRE option is to prevent locking the app/stream when OvenMediaEngine unexpectedly stops.
		// So _update_timer updates the expire time once every 2.5 seconds.
		redisReply *reply = (redisReply *)redisCommand(_redis_context, "SET %s %s EX %d NX", app_stream_name.CStr(), origin_host.CStr(), ORIGIN_MAP_STORE_KEY_EXPIRE_TIME);
		if (reply == nullptr || reply->type == REDIS_REPLY_ERROR)
		{
			logte("Failed to set origin host to redis : %s:%d (err:%s)", _redis_ip.CStr(), _redis_port, reply!=nullptr?reply->str:"nil");
			return false;
		}
		else if (reply->type == REDIS_REPLY_NIL)
		{
			logte("<%s> stream is already registered.", app_stream_name.CStr());
			freeReplyObject(reply);
			lock.unlock();

			std::lock_guard<std::mutex> origin_map_lock(_origin_map_mutex);
			_origin_map_candidates[app_stream_name] = origin_host;
			return false;
		}

		freeReplyObject(reply);
	}

	lock.unlock();

	std::lock_guard<std::mutex> origin_map_lock(_origin_map_mutex);
	_origin_map[app_stream_name] = origin_host;

	logti("OriginMapStore: <%s> stream is registered with origin host : %s", app_stream_name.CStr(), origin_host.CStr());

	return true;
}

bool OriginMapClient::Update(const ov::String &app_stream_name, const ov::String &origin_host)
{
	if (ConnectRedis() == false)
	{
		logte("Failed to connect redis server : %s:%d (err:%s)", _redis_ip.CStr(), _redis_port, _redis_context!=nullptr?_redis_context->errstr:"nil");
		return false;
	}

	std::lock_guard<std::mutex> lock(_redis_context_mutex);

	// Set origin host to redis
	// XX option or EXPIRE cmd are not used because if redis server is restarted, update() can restore the origin stream info.
	redisReply *reply = (redisReply *)redisCommand(_redis_context, "SET %s %s EX %d", app_stream_name.CStr(), origin_host.CStr(), ORIGIN_MAP_STORE_KEY_EXPIRE_TIME);
	if (reply == nullptr || reply->type == REDIS_REPLY_ERROR)
	{
		logte("Failed to set origin host to redis : %s:%d (err:%s)", _redis_ip.CStr(), _redis_port, reply!=nullptr?reply->str:"nil");
		return false;
	}
	else if (reply->type == REDIS_REPLY_NIL)
	{
		// Not exist
		return false;
	}

	freeReplyObject(reply);

	return true;
}

bool OriginMapClient::Unregister(const ov::String &app_stream_name)
{
	if (ConnectRedis() == false)
	{
		logte("Failed to connect redis server : %s:%d (err:%s)", _redis_ip.CStr(), _redis_port, _redis_context!=nullptr?_redis_context->errstr:"nil");
		return false;
	}

	std::unique_lock<std::mutex> lock(_redis_context_mutex);

	redisReply *reply = (redisReply *)redisCommand(_redis_context, "DEL %s", app_stream_name.CStr());
	freeReplyObject(reply);

	lock.unlock();

	std::lock_guard<std::mutex> origin_map_lock(_origin_map_mutex);
	_origin_map.erase(app_stream_name);

	logti("OriginMapStore: <%s> stream is unregistered.", app_stream_name.CStr());

	return true;
}

CommonErrorCode OriginMapClient::GetOrigin(const ov::String &app_stream_name, ov::String &origin_host)
{
	if (ConnectRedis() == false)
	{
		logte("Failed to connect redis server : %s:%d (err:%s)", _redis_ip.CStr(), _redis_port, _redis_context!=nullptr?_redis_context->errstr:"nil");
		return CommonErrorCode::ERROR;
	}

	std::lock_guard<std::mutex> lock(_redis_context_mutex);

	redisReply *reply = (redisReply *)redisCommand(_redis_context, "GET %s", app_stream_name.CStr());
	if (reply == nullptr || reply->type == REDIS_REPLY_ERROR)
	{
		logte("Failed to get origin host from redis : %s:%d (err:%s)", _redis_ip.CStr(), _redis_port, reply!=nullptr?reply->str:"nil");
		return CommonErrorCode::ERROR;
	}
	else if (reply->type == REDIS_REPLY_NIL)
	{
		return CommonErrorCode::NOT_FOUND;
	}

	origin_host = reply->str;
	freeReplyObject(reply);

	return CommonErrorCode::SUCCESS;
}

bool OriginMapClient::ConnectRedis()
{
	std::lock_guard<std::mutex> lock(_redis_context_mutex);

	if (CheckConnection() == true)
	{
		return true;
	}

	// connect to redis server
	_redis_context = redisConnect(_redis_ip.CStr(), _redis_port);
	if (_redis_context == nullptr || _redis_context->err)
	{
		if (_redis_context == nullptr)
		{
			redisFree(_redis_context);
			_redis_context = nullptr;
		}

		logte("Failed to connect to redis server. ip: %s, port: %d, err: %s", _redis_ip.CStr(), _redis_port, _redis_context->errstr);
		return false;
	}

	// Auth
	if (_redis_password.IsEmpty() == false)
	{
		redisReply *reply = (redisReply *)redisCommand(_redis_context, "AUTH %s", _redis_password.CStr());
		if (reply == nullptr || reply->type == REDIS_REPLY_ERROR)
		{
			logte("Failed to auth to redis server. ip: %s, port: %d, err: %s", _redis_ip.CStr(), _redis_port, reply!=nullptr?reply->str:"nil");

			if (reply != nullptr)
			{
				freeReplyObject(reply);
			}
			
			return false;
		}
	}

	return true;
}

bool OriginMapClient::CheckConnection()
{
	if (_redis_context == nullptr)
	{
		return false;
	}

	// Test with PING command
	redisReply *reply = (redisReply *)redisCommand(_redis_context, "PING");
	if (reply == nullptr)
	{
		return false;
	}

	return true;
}
