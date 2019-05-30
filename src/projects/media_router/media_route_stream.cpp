//==============================================================================
//
//  MediaRouteStream
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "media_route_stream.h"

#include <base/ovlibrary/ovlibrary.h>

#define OV_LOG_TAG "MediaRouter.Stream"

using namespace common;

MediaRouteStream::MediaRouteStream(std::shared_ptr<StreamInfo> stream_info)
{
	logtd("Trying to create media route stream: name(%s) id(%u)", stream_info->GetName().CStr(), stream_info->GetId());

	// 스트림 정보 저정
	_stream_info = stream_info;
	_stream_info->ShowInfo();

	time(&_last_rb_time);
}

MediaRouteStream::~MediaRouteStream()
{
	logtd("Delete media route stream name(%s) id(%u)", _stream_info->GetName().CStr(), _stream_info->GetId());
}

std::shared_ptr<StreamInfo> MediaRouteStream::GetStreamInfo()
{
	return _stream_info;
}

void MediaRouteStream::SetConnectorType(MediaRouteApplicationConnector::ConnectorType type)
{
	_application_connector_type = type;
}

MediaRouteApplicationConnector::ConnectorType MediaRouteStream::GetConnectorType()
{
	return _application_connector_type;
}

bool MediaRouteStream::Push(std::unique_ptr<MediaPacket> buffer, bool convert_bitstream)
{
	MediaType media_type = buffer->GetMediaType();
	int32_t track_id = buffer->GetTrackId();
	auto media_track = _stream_info->GetTrack(track_id);

	if(media_track == nullptr)
	{
		logte("Cannot find media track. type(%s), id(%d)", (media_type == MediaType::Video) ? "video" : "audio", track_id);
		return false;
	}

	if(convert_bitstream)
	{
		if(media_type == MediaType::Video && media_track->GetCodecId() == MediaCodecId::H264)
		{
		    int64_t cts = 0;

		    _bsfv.convert_to(buffer->GetData(), cts);
            buffer->SetCts(cts);

            // logtd("rtmp input h264 pts(%lld) cts(%lld)", buffer->GetPts(), buffer->GetCts());
		}
		else if(media_type == MediaType::Video && media_track->GetCodecId() == MediaCodecId::Vp8)
		{
			_bsf_vp8.convert_to(buffer->GetData());
		}
		else if(media_type == MediaType::Audio && media_track->GetCodecId() == MediaCodecId::Aac)
		{
			_bsfa.convert_to(buffer->GetData());
			logtp("Enqueue for AAC\n%s", buffer->GetData()->Dump(32).CStr());
		}
		else if(media_type == MediaType::Audio && media_track->GetCodecId() == MediaCodecId::Opus)
		{
			// logtw("%s", buffer->GetData()->Dump(32).CStr());
			// _bsfa.convert_to(buffer.GetBuffer());
			logtp("Enqueue for OPUS\n%s", buffer->GetData()->Dump(32).CStr());
		}
		else
		{
			OV_ASSERT2(false);
		}
	}

#if 0
	buffer->SetPts(media_track->GetStartFrameTime() + buffer->GetPts());
	
	// 수신된 버퍼의 PTS값이 초기화되면 마지막에 받은 PTS값을 기준으로 증가시켜준다
	if( (media_track->GetLastFrameTime()-1000000) > buffer->GetPts())
	{
		logtw("change start time");
		logtd("start buffer : %.0f, last buffer : %.0f, buffer : %.0f", (float)media_track->GetStartFrameTime(), (float)media_track->GetLastFrameTime(), (float)buffer->GetPts());

		media_track->SetStartFrameTime(media_track->GetLastFrameTime());
	}

	media_track->SetLastFrameTime(buffer->GetPts());
#endif

	// 변경된 스트림을 큐에 넣음
	_queue.push(std::move(buffer));

	time(&_last_rb_time);
	// logtd("last time : %s", asctime(gmtime(&_last_rb_time)) );

	return true;
}

std::unique_ptr<MediaPacket> MediaRouteStream::Pop()
{
	if(_queue.empty())
	{
		return nullptr;
	}

	auto p2 = std::move(_queue.front());
	_queue.pop();

	return p2;
}

uint32_t MediaRouteStream::Size()
{
	return _queue.size();
}


time_t MediaRouteStream::getLastReceivedTime()
{
	return _last_rb_time;
}