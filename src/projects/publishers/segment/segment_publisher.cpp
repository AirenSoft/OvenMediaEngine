//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#include "segment_publisher.h"
#include "publisher_private.h"

#include <orchestrator/orchestrator.h>
#include <publishers/segment/segment_stream/segment_stream.h>
#include <monitoring/monitoring.h>

SegmentPublisher::SegmentPublisher(const cfg::Server &server_config, const info::Host &host_info, const std::shared_ptr<MediaRouteInterface> &router)
	: Publisher(server_config, host_info, router)
{
}

SegmentPublisher::~SegmentPublisher()
{
	_run_thread = false;
	logtd("Publisher has been destroyed");
}

bool SegmentPublisher::GetMonitoringCollectionData(std::vector<std::shared_ptr<pub::MonitoringCollectionData>> &collections)
{
	return (_stream_server != nullptr) ? _stream_server->GetMonitoringCollectionData(collections) : false;
}

bool SegmentPublisher::OnPlayListRequest(const std::shared_ptr<HttpClient> &client,
										 const ov::String &app_name, const ov::String &stream_name,
										 const ov::String &file_name,
										 ov::String &play_list)
{
	auto stream = GetStreamAs<SegmentStream>(app_name, stream_name);

	if ((stream == nullptr) || (stream->GetPlayList(play_list) == false))
	{
		logtw("Could not get a playlist for %s [%p, %s/%s, %s]", GetPublisherName(), stream.get(), app_name.CStr(), stream_name.CStr(), file_name.CStr());
		return false;
	}

	return true;
}

bool SegmentPublisher::OnSegmentRequest(const std::shared_ptr<HttpClient> &client,
										const ov::String &app_name, const ov::String &stream_name,
										const ov::String &file_name,
										std::shared_ptr<SegmentData> &segment)
{
	auto stream = GetStreamAs<SegmentStream>(app_name, stream_name);

	if (stream != nullptr)
	{
		segment = stream->GetSegmentData(file_name);

		if (segment == nullptr)
		{
			logtw("Could not find a segment for %s [%s/%s, %s]", GetPublisherName(), app_name.CStr(), stream_name.CStr(), file_name.CStr());
			return false;
		}
		else if (segment->data == nullptr)
		{
			logtw("Could not obtain segment data from %s for [%p, %s/%s, %s]", GetPublisherName(), segment.get(), app_name.CStr(), stream_name.CStr(), file_name.CStr());
			return false;
		}
	}
	else
	{
		logtw("Could not find a stream for %s [%s/%s, %s]", GetPublisherName(), app_name.CStr(), stream_name.CStr(), file_name.CStr());
		return false;
	}

	// To manage sessions
	AddSessionRequestInfo(SessionRequestInfo(GetPublisherType(), 
											*std::static_pointer_cast<info::Stream>(stream), 
											client->GetRemote()->GetRemoteAddress()->GetIpAddress(), 
											segment->sequence_number,
											segment->duration));

	return true;
}

bool SegmentPublisher::StartSessionTableManager()
{
	_run_thread = true;
	_worker_thread = std::thread(&SegmentPublisher::SessionTableUpdateThread, this);
	_worker_thread.detach();

	return true;
}

void SegmentPublisher::SessionTableUpdateThread()
{
	while(_run_thread)
	{
		// Remove a quite old session request info
		// Only HLS/DASH/CMAF publishers have some items.
		std::unique_lock<std::recursive_mutex> table_lock(_session_table_lock);

		for(auto item = _session_table.begin(); item != _session_table.end();)
		{
			auto request_info = item->second;
			if(request_info->IsExpiredRequest())
			{
				// remove and report
				auto stream_metrics = StreamMetrics(request_info->GetStreamInfo());
				if(stream_metrics != nullptr)
				{
					stream_metrics->OnSessionDisconnected(request_info->GetPublisherType());
				}

				item = _session_table.erase(item);
			}
			else
			{
				++ item;
			}
		}

		table_lock.unlock();
		sleep(3);
	}
}

void SegmentPublisher::AddSessionRequestInfo(const SessionRequestInfo &info)
{
	bool new_session = true;
	std::unique_lock<std::recursive_mutex> table_lock(_session_table_lock);

	auto select_count = _session_table.count(info.GetIpAddress().CStr());
	if(select_count > 0)
	{
		// select * where IP=info.ip from _session_table
		auto it = _session_table.equal_range(info.GetIpAddress().CStr());
		for (auto itr = it.first; itr != it.second;) 
		{ 
        	auto item = itr->second;

			if(item->IsNextRequest(info))
			{
				itr = _session_table.erase(itr);
				new_session = false;
				break;
			}
			else
			{
				++ itr;
			}
    	}
	}

	// It is a new viewer!
	if(new_session)
	{
		auto stream_metrics = StreamMetrics(info.GetStreamInfo());
		if(stream_metrics != nullptr)
		{
			stream_metrics->OnSessionConnected(info.GetPublisherType());
		}
	}

	_session_table.insert(std::pair<std::string, std::shared_ptr<SessionRequestInfo>>(info.GetIpAddress().CStr(), std::make_shared<SessionRequestInfo>(info)));
}