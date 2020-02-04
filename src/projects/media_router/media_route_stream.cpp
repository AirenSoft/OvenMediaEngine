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

MediaRouteStream::MediaRouteStream(std::shared_ptr<info::Stream> &stream)
{
	logtd("Trying to create media route stream: name(%s) id(%u)", stream->GetName().CStr(), stream->GetId());

	_stream = stream;
	_stream->ShowInfo();

	time(&_last_rb_time);
}

MediaRouteStream::~MediaRouteStream()
{
	logtd("Delete media route stream name(%s) id(%u)", _stream->GetName().CStr(), _stream->GetId());
}

std::shared_ptr<info::Stream> MediaRouteStream::GetStream()
{
	return _stream;
}

void MediaRouteStream::SetConnectorType(MediaRouteApplicationConnector::ConnectorType type)
{
	_application_connector_type = type;
}

MediaRouteApplicationConnector::ConnectorType MediaRouteStream::GetConnectorType()
{
	return _application_connector_type;
}

// 비트스트림 컨버팅 기능을.. 어디에 넣는게 좋을까? Push? Pop?
bool MediaRouteStream::Push(std::shared_ptr<MediaPacket> media_packet)
{
	auto media_type = media_packet->GetMediaType();

	auto track_id = media_packet->GetTrackId();

	auto media_track = _stream->GetTrack(track_id);

	if (media_track == nullptr)
	{
		logte("Cannot find media track. media_type(%s), track_id(%d)", (media_type == MediaType::Video) ? "video" : "audio", media_packet->GetTrackId());
		return false;
	}


#if 0 // for debug...
        if(_stream->GetName() == "livestream_o2" || _stream->GetName() == "livestream_o")
        {
                if(media_type==MediaType::Video)
                        _last_video_pts = media_packet->GetPts() * 1000 / media_track->GetTimeBase().GetDen();
                else
                        _last_audio_pts = media_packet->GetPts() * 1000 / media_track->GetTimeBase().GetDen();

                logtd("name(%10s) tid(%2d) type(%s), lvpts(%10lld), lapts(%10lld) diff(%5lld)",
                 _stream->GetName().CStr(), track_id, (media_type==MediaType::Video)?"Video":"Audio",
                 _last_video_pts, _last_audio_pts, _last_video_pts - _last_audio_pts);
        }
#endif


	if (media_type == MediaType::Video)
	{
		if (media_track->GetCodecId() == MediaCodecId::H264)
		{
			// 만약, RTP 프레그멘테이션을 위한 헤더 정보가 없다면 생성한다.
			if(media_packet->GetFragHeader()->GetCount() == 0)
			{
				_avc_video_fragmentizer.MakeHeader(media_packet);
			}
		}
		else if (media_track->GetCodecId() == MediaCodecId::Vp8)
		{
			// TODO: Vp8 코덱과 같은 경우에는 Provider로 나중에 옮겨야 겠음.
			_bsf_vp8.convert_to(media_packet->GetData());
		}
		else
		{
			logte("Unsupported video codec. codec_id(%d)", media_track->GetCodecId());
			return false;
		}
	}
	else if (media_type == MediaType::Audio)
	{
		if (media_track->GetCodecId() == MediaCodecId::Aac)
		{
		}
		else if (media_track->GetCodecId() == MediaCodecId::Opus)
		{

		}
		else
		{
			logte("Unsupported audio codec. codec_id(%d)", media_track->GetCodecId());
			return false;
		}
	}
	else
	{
		logte("Unsupported media typec. media_type(%d)", media_type);
		return false;
	}

	_media_packets.push(std::move(media_packet));


	// Purpose of keeping time for last received packets.
	// In the future, it will be utilized in the wrong stream or garbage collector.

	// time(&_last_rb_time);
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
