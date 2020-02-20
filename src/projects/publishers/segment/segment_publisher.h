//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/common_types.h>
#include <base/media_route/media_route_application_interface.h>
#include <base/publisher/publisher.h>
#include <config/config.h>
#include <publishers/segment/segment_stream/segment_stream_server.h>

#define DEFAULT_SEGMENT_WORKER_THREAD_COUNT 4

class SessionRequestInfo
{
public:
	SessionRequestInfo(const PublisherType &type, const info::Stream &stream_info, const ov::String &ip, int seq, int64_t duration)
		: _stream_info(stream_info)
	{
		_publisher_type = type;
		_session_ip_address = ip;
		_sequence_number = seq;
		_duration = duration;
		_last_requested_time = std::chrono::system_clock::now();
	}

	bool IsNextRequest(const SessionRequestInfo &next)
	{
		// Is this a series of requests?
		if(_publisher_type == next._publisher_type && 
			_stream_info == next._stream_info && 
			_session_ip_address == next._session_ip_address && 
			_sequence_number + 1 == next._sequence_number)
		{
			auto gap = std::chrono::duration_cast<std::chrono::seconds>(_last_requested_time - next._last_requested_time).count();
			
			// the next request comes in within a short time
			if(gap < _duration * 2)
			{
				return true;
			}
		}
		return false;
	}

	const PublisherType& GetPublisherType() const { return _publisher_type; }
	const info::Stream& GetStreamInfo() const { return _stream_info; }
	const ov::String& GetIpAddress() const { return _session_ip_address; }
	int	GetSequenceNumber() { return _sequence_number; }
	int64_t GetDuration(){ return _duration; }

private:
	PublisherType		_publisher_type;
	info::Stream		_stream_info;
	ov::String			_session_ip_address;
	int					_sequence_number;
	int64_t				_duration;
	std::chrono::system_clock::time_point	_last_requested_time;
};

class SegmentPublisher : public pub::Publisher, public SegmentStreamObserver
{
protected:
	// This is used to prevent create new instance without factory class
	struct PrivateToken
	{
	};

public:
	template <typename Tpublisher>
	static std::shared_ptr<Tpublisher> Create(std::map<int, std::shared_ptr<HttpServer>> &http_server_manager,
											  const cfg::Server &server_config,
											  const info::Host &host_info,
											  const std::shared_ptr<MediaRouteInterface> &router)
	{
		auto publisher = std::make_shared<Tpublisher>((PrivateToken){}, server_config, host_info, router);

		auto instance = std::static_pointer_cast<SegmentPublisher>(publisher);
		if (instance->Start(http_server_manager) == false)
		{
			return nullptr;
		}

		return publisher;
	}

	bool GetMonitoringCollectionData(std::vector<std::shared_ptr<pub::MonitoringCollectionData>> &collections) override;

protected:
	SegmentPublisher(const cfg::Server &server_config, const info::Host &host_info, const std::shared_ptr<MediaRouteInterface> &router);
	~SegmentPublisher() override;

	virtual bool Start(std::map<int, std::shared_ptr<HttpServer>> &http_server_manager) = 0;

	//--------------------------------------------------------------------
	// Implementation of SegmentStreamObserver
	//--------------------------------------------------------------------
	bool OnPlayListRequest(const std::shared_ptr<HttpClient> &client,
						   const ov::String &app_name, const ov::String &stream_name,
						   const ov::String &file_name,
						   ov::String &play_list) override;

	bool OnSegmentRequest(const std::shared_ptr<HttpClient> &client,
						  const ov::String &app_name, const ov::String &stream_name,
						  const ov::String &file_name,
						  std::shared_ptr<SegmentData> &segment) override;

	std::shared_ptr<SegmentStreamServer> _stream_server = nullptr;

private:
	void		AddSessionRequestInfo(const info::Stream &stream_info, const SessionRequestInfo &info);
	void 		SessionTableUpdateThread();

	// key: ip address, Probabliy, the ip address won't be the most duplicated in the session table.
	std::multimap<std::string, std::shared_ptr<SessionRequestInfo>>	_session_table[static_cast<int8_t>(PublisherType::NumberOfPublishers)];
};
