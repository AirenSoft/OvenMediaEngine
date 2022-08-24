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

bool OriginMapClient::ConnectRedis(const ov::String &redis_host, const ov::String &redis_password)
{
	// Parse ip:port 
	auto ip_port = redis_host.Split(":");
	if (ip_port.size() != 2)
	{
		return false;
	}

	_redis_ip = ip_port[0];
	_redis_port = ov::Converter::ToUInt16(ip_port[1]);
	_redis_password = redis_password;

	return ConnectRedis();
}

bool OriginMapClient::Reigster(const ov::String &app_stream_name, const ov::String &origin_host)
{
	if (ConnectRedis() == false)
	{
		logte("Failed to connect redis server : %s:%d (err:%s)", _redis_ip.CStr(), _redis_port, _redis_context!=nullptr?_redis_context->errstr:"nil");
		return false;
	}

	// Set origin host to redis
	redisReply *reply = (redisReply *)redisCommand(_redis_context, "SET %s %s EX 10 NX", app_stream_name.CStr(), origin_host.CStr());
	if (reply == nullptr || reply->type == REDIS_REPLY_NIL || reply->type == REDIS_REPLY_ERROR)
	{
		logte("Failed to set origin host to redis : %s:%d (err:%s)", _redis_ip.CStr(), _redis_port, reply!=nullptr?reply->str:"nil");
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

	redisReply *reply = (redisReply *)redisCommand(_redis_context, "DEL %s", app_stream_name.CStr());
	freeReplyObject(reply);

	return true;
}

bool OriginMapClient::GetOrigin(const ov::String &app_stream_name, ov::String &origin_host)
{
	if (ConnectRedis() == false)
	{
		logte("Failed to connect redis server : %s:%d (err:%s)", _redis_ip.CStr(), _redis_port, _redis_context!=nullptr?_redis_context->errstr:"nil");
		return false;
	}

	redisReply *reply = (redisReply *)redisCommand(_redis_context, "GET %s", app_stream_name.CStr());
	if (reply == nullptr || reply->type == REDIS_REPLY_NIL || reply->type == REDIS_REPLY_ERROR)
	{
		logte("Failed to get origin host from redis : %s:%d (err:%s)", _redis_ip.CStr(), _redis_port, reply!=nullptr?reply->str:"nil");
		return false;
	}
	origin_host = reply->str;
	freeReplyObject(reply);
	return true;
}

bool OriginMapClient::ConnectRedis()
{
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