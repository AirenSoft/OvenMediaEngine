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

SegmentPublisher::SegmentPublisher(const cfg::Server &server_config, const info::Host &host_info, const std::shared_ptr<MediaRouteInterface> &router)
	: Publisher(server_config, host_info, router)
{
}

SegmentPublisher::~SegmentPublisher()
{
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

	// These names are used for testing purposes
	// TODO(dimiden): Need to delete this code after testing
	if (stream == nullptr)
	{
		if (app_name.HasSuffix("#rtsp_live") || app_name.HasSuffix("#rtsp_playback"))
		{
			auto &request = client->GetRequest();
			auto host_name = request->GetHeader("HOST").Split(":")[0];
			auto uri = request->GetUri();
			auto parsed_url = ov::Url::Parse(uri.CStr(), true);

			if (parsed_url == nullptr)
			{
				logte("Could not parse the url: %s", uri.CStr());
				return false;
			}

			auto &query_map = parsed_url->QueryMap();
			auto rtsp_uri_item = query_map.find("rtspURI");

			if (rtsp_uri_item == query_map.end())
			{
				logte("There is no rtspURI parameter in the query string: %s", uri.CStr());
				logtd("Query map:");
				for(auto &query : query_map)
				{
					logtd("    %s = %s", query.first.CStr(), query.second.CStr());
				}
				return false;
			}

			auto orchestrator = Orchestrator::GetInstance();
			auto rtsp_uri = rtsp_uri_item->second;

			if (orchestrator->RequestPullStream(app_name, stream_name, rtsp_uri) == false)
			{
				logte("Could not request pull stream for URL: %s", rtsp_uri.CStr());
				return false;
			}

			logti("URL %s is requested");
		}
	}

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

	return true;
}
