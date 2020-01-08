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

// 비트스트림 컨버팅 기능을.. 어디에 넣는게 좋을까? Push? Pop?
bool MediaRouteStream::Push(std::shared_ptr<MediaPacket> media_packet, bool convert_bitstream)
{
	auto media_type = media_packet->GetMediaType();

	auto track_id = media_packet->GetTrackId();

	auto media_track = _stream_info->GetTrack(track_id);

	if (media_track == nullptr)
	{
		logte("Cannot find media track. media_type(%s), track_id(%d)", (media_type == MediaType::Video) ? "video" : "audio", media_packet->GetTrackId());
		return false;
	}

	// TODO(soulk): Move this code to RTMP provider/Transcoder (RTMP need to calculate pts using dts and cts)
	if (convert_bitstream)
	{
		if (media_type == MediaType::Video)
		{
			if (media_track->GetCodecId() == MediaCodecId::H264)
			{

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