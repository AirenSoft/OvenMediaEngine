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
#include <base/mediarouter/media_route_application_interface.h>
#include <base/publisher/publisher.h>
#include <config/config.h>
#include <publishers/segment/segment_stream/segment_stream_server.h>

#define DEFAULT_SEGMENT_WORKER_THREAD_COUNT 4

// It is used to determine if the token has expired but is an authorized session.
class PlaylistRequestInfo
{
public:
	PlaylistRequestInfo(const PublisherType &type, const info::VHostAppName &vhost_app_name, const ov::String &stream_name, const ov::String &ip, const ov::String &session_id)
	{
		_publisher_type = type;
		_session_id = session_id;
		_vhost_app_name = vhost_app_name;
		_stream_name = stream_name;
		_ip_address = ip;
		_last_requested_time = std::chrono::system_clock::now();
	}

	PlaylistRequestInfo(const PlaylistRequestInfo &info)
	{
		_publisher_type = info._publisher_type;
		_session_id = info._session_id;
		_vhost_app_name = info._vhost_app_name;
		_stream_name = info._stream_name;
		_ip_address = info._ip_address;
		_last_requested_time = info._last_requested_time;
	}

	bool IsTooOld()
	{
		auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - _last_requested_time).count();
		// TODO(Getroot): It should be related with segment duration.
		if(elapsed > 30) 
		{
			return true;
		}

		return false;
	}

	bool IsRequestFromSameUser(const PlaylistRequestInfo &info)
	{
		if(_publisher_type == info._publisher_type &&
			_vhost_app_name == info._vhost_app_name &&
			_stream_name == info._stream_name &&
			_session_id == info._session_id &&
			_ip_address == info._ip_address)
		{
			return true;
		}

		return false;
	}

	const PublisherType& GetPublisherType() const { return _publisher_type; }
	const ov::String& GetIpAddress() const { return _ip_address; }
	const ov::String& GetSessionId() const { return _session_id; }
	const info::VHostAppName& GetAppName() const { return _vhost_app_name; }
	const ov::String& GetStreamName() const { return _stream_name; }

private:
	PublisherType		_publisher_type;
	info::VHostAppName	_vhost_app_name;
	ov::String			_stream_name;
	ov::String			_ip_address;
	ov::String			_session_id;
	std::chrono::system_clock::time_point	_last_requested_time;
};

class SegmentRequestInfo
{
public:
	SegmentRequestInfo(const PublisherType &type, const info::Stream &stream_info, const ov::String &ip, int seq, int64_t duration)
		: _stream_info(stream_info)
	{
		_publisher_type = type;
		_ip_address = ip;
		_sequence_number = seq;
		_duration = (double)duration / (double)(PACKTYZER_DEFAULT_TIMESCALE); // convert to second
		_last_requested_time = std::chrono::system_clock::now();
		_count = 0;
	}

	SegmentRequestInfo(const SegmentRequestInfo &info)
		: _stream_info(info._stream_info)
	{
		_publisher_type = info._publisher_type;
		_ip_address = info._ip_address;
		_sequence_number = info._sequence_number;
		_duration = info._duration;
		_last_requested_time = info._last_requested_time;
		_count = info._count;
	}

	bool IsNextRequest(const SegmentRequestInfo &next)
	{
		// Is this a series of requests?
		if(_publisher_type == next._publisher_type && 
			_stream_info == next._stream_info && 
			_ip_address == next._ip_address && 
			_sequence_number + 1 == next._sequence_number)
		{
			// Not measuring the time interval between segment requests will give more accurate results when the network is bad.
			return true;

			/*TODO(Getroot): Check again how effective this function is
			auto gap = std::chrono::duration_cast<std::chrono::seconds>(next._last_requested_time - _last_requested_time).count();
			// the next request comes in within a short time
			if(gap < _duration * 5)
			{
				return true;
			}
			*/
		}
		return false;
	}

	bool IsExpiredRequest()
	{
		auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - _last_requested_time).count();
		if(elapsed > _duration * 10)
		{
			return true;
		}

		return false;
	}

	const ov::String GetTimeStr() const
	{
		return ov::Converter::ToString(_last_requested_time);
	}

	void SetCount(uint32_t count)
	{
		_count = count;
	}

	const PublisherType& GetPublisherType() const { return _publisher_type; }
	const info::Stream& GetStreamInfo() const { return _stream_info; }
	const ov::String& GetIpAddress() const { return _ip_address; }
	int	GetSequenceNumber() const { return _sequence_number; }
	int64_t GetDuration() const { return _duration; }
	uint32_t GetCount() const { return _count; }

private:
	PublisherType		_publisher_type;
	info::Stream		_stream_info;
	ov::String			_ip_address;
	int					_sequence_number;
	int64_t				_duration;
	uint32_t			_count = 0;
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
											  const std::shared_ptr<MediaRouteInterface> &router)
	{
		auto publisher = std::make_shared<Tpublisher>((PrivateToken){}, server_config, router);

		auto instance = std::static_pointer_cast<SegmentPublisher>(publisher);
		if (instance->Start(http_server_manager) == false)
		{
			return nullptr;
		}

		if (instance->StartSessionTableManager() == false)
		{
			return nullptr;
		}

		return publisher;
	}

	bool GetMonitoringCollectionData(std::vector<std::shared_ptr<pub::MonitoringCollectionData>> &collections) override;

	bool Stop() override;

protected:
	SegmentPublisher(const cfg::Server &server_config, const std::shared_ptr<MediaRouteInterface> &router);
	~SegmentPublisher() override;

	bool Start(std::map<int, std::shared_ptr<HttpServer>> &http_server_manager, const cfg::SingularPort &port_config, const cfg::SingularPort &tls_port_config, const std::shared_ptr<SegmentStreamServer> &stream_server);
	virtual bool Start(std::map<int, std::shared_ptr<HttpServer>> &http_server_manager) = 0;
	

	bool HandleSignedUrl(const info::VHostAppName &vhost_app_name, const ov::String &stream_name, 
						const std::shared_ptr<HttpClient> &client, const std::shared_ptr<const ov::Url> &request_url,
						std::shared_ptr<PlaylistRequestInfo> &request_info);

	//--------------------------------------------------------------------
	// Implementation of SegmentStreamObserver
	//--------------------------------------------------------------------
	bool OnPlayListRequest(const std::shared_ptr<HttpClient> &client,
						   const SegmentStreamRequestInfo &request_info,
						   ov::String &play_list) override;

	bool OnSegmentRequest(const std::shared_ptr<HttpClient> &client,
						  const SegmentStreamRequestInfo &request_info,
						  std::shared_ptr<SegmentData> &segment) override;

	std::shared_ptr<SegmentStreamServer> _stream_server = nullptr;

private:
	bool		StartSessionTableManager();
	void 		RequestTableUpdateThread();

	void		UpdatePlaylistRequestInfo(const std::shared_ptr<PlaylistRequestInfo> &info);
	const std::shared_ptr<PlaylistRequestInfo>	GetSessionRequestInfoBySegmentRequestInfo(const SegmentRequestInfo &info);
	bool		IsAuthorizedSession(const PlaylistRequestInfo &info);

	void		UpdateSegmentRequestInfo(SegmentRequestInfo &info);

	bool					_run_thread = false;
	std::recursive_mutex 	_playlist_request_table_lock;
	std::recursive_mutex 	_segment_request_table_lock;
	std::thread 			_worker_thread;

	// key: session id, Probabliy, the session_id will be the least duplicated info in the _playlist_request_table.
	std::map<std::string, std::shared_ptr<PlaylistRequestInfo>>			_playlist_request_table;
	// key: ip address, Probabliy, the ip address will be the least duplicated info in the _segment_request_table.
	std::multimap<std::string, std::shared_ptr<SegmentRequestInfo>>		_segment_request_table;
};
