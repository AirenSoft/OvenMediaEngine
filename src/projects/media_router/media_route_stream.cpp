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

MediaRouteStream::MediaRouteStream(std::shared_ptr<StreamInfo> &stream_info)
{
	logtd("Trying to create media route stream: name(%s) id(%u)", stream_info->GetName().CStr(), stream_info->GetId());

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

bool MediaRouteStream::Push(std::shared_ptr<MediaPacket> media_packet, bool convert_bitstream)
{
	MediaType media_type = media_packet->GetMediaType();
	int32_t track_id = media_packet->GetTrackId();
	auto media_track = _stream_info->GetTrack(track_id);

	if(media_track == nullptr)
	{
		logte("Cannot find media track. type(%s), id(%d)", (media_type == MediaType::Video) ? "video" : "audio", track_id);
		return false;
	}

	// TODO(soulk): Move this code to RTMP provider/Transcoder (RTMP need to calculate pts using dts and cts)
	if(convert_bitstream)
	{
		if(media_type == MediaType::Video && media_track->GetCodecId() == MediaCodecId::H264)
		{
		    int64_t cts = 0;

			if(_bsfv.Convert(media_packet->GetData(), cts) == false)
			{
				return false;
			}

			// The timestamp used by RTMP is DTS
			// cts = pts - dts
			// pts = dts + cts
			// Convert timebase 1/1000 to 1/90000
			media_packet->SetPts(media_packet->GetDts() + (cts * 90));

			// OV_ASSERT2(media_packet->GetPts() >= 0LL);
		}
		else if(media_type == MediaType::Video && media_track->GetCodecId() == MediaCodecId::Vp8)
		{
			_bsf_vp8.convert_to(media_packet->GetData());
		}
		else if(media_type == MediaType::Audio && media_track->GetCodecId() == MediaCodecId::Aac)
		{
			_bsfa.convert_to(media_packet->GetData());

			logtp("Enqueue for AAC\n%s", media_packet->GetData()->Dump(32).CStr());
		}
		else if(media_type == MediaType::Audio && media_track->GetCodecId() == MediaCodecId::Opus)
		{
			// logtw("%s", media_packet->GetData()->Dump(32).CStr());
			// _bsfa.convert_to(media_packet.GetBuffer());
			logtp("Enqueue for OPUS\n%s", media_packet->GetData()->Dump(32).CStr());
		}
		else
		{
			OV_ASSERT2(false);
		}
	}

#if 0
	media_packet->SetPts(media_track->GetStartFrameTime() + media_packet->GetPts());
	
	// 수신된 버퍼의 PTS값이 초기화되면 마지막에 받은 PTS값을 기준으로 증가시켜준다
	if( (media_track->GetLastFrameTime()-1000000) > media_packet->GetPts())
	{
		logtw("change start time");
		logtd("start media_packet : %.0f, last media_packet : %.0f, media_packet : %.0f", (float)media_track->GetStartFrameTime(), (float)media_track->GetLastFrameTime(), (float)media_packet->GetPts());

		media_track->SetStartFrameTime(media_track->GetLastFrameTime());
	}

	media_track->SetLastFrameTime(media_packet->GetPts());
#endif

	_media_packets.push(std::move(media_packet));

	time(&_last_rb_time);
	// logtd("last time : %s", asctime(gmtime(&_last_rb_time)) );

	return true;
}

std::shared_ptr<MediaPacket> MediaRouteStream::Pop()
{
	if(_media_packets.empty())
	{
		return nullptr;
	}

	auto p2 = std::move(_media_packets.front());
	_media_packets.pop();

	return p2;
}

uint32_t MediaRouteStream::Size()
{
	return _media_packets.size();
}


time_t MediaRouteStream::getLastReceivedTime()
{
	return _last_rb_time;
}