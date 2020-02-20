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

#define PTS_CORRECT_THRESHOLD_US	5000	

MediaRouteStream::MediaRouteStream(std::shared_ptr<info::Stream> &stream)
{
	logtd("Trying to create media route stream: name(%s) id(%u)", stream->GetName().CStr(), stream->GetId());

	_stream = stream;
	_stream->ShowInfo();

	time(&_stat_start_time);
	time(&_last_recv_time);
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


	// PTS Correction for Abnormal increase

	int64_t scaled_pts_new = media_packet->GetPts() * 1000 / media_track->GetTimeBase().GetDen();
	
	int64_t scaled_pts_lastest = _stat_recv_pkt_lpts[track_id] * 1000 / media_track->GetTimeBase().GetDen();
	
	int64_t scaled_delta = scaled_pts_new - scaled_pts_lastest;

	if(abs( scaled_delta ) > PTS_CORRECT_THRESHOLD_US )
	{
		_pts_correct[track_id] = media_packet->GetPts() - _stat_recv_pkt_lpts[track_id] - _pts_avg_inc[track_id];
		logtw("Detected abnormal increased pts. track_id : %d, pts : %lld, avginc : %lld, corrected pts : %lld", track_id,  scaled_pts_new, _pts_avg_inc[track_id], _pts_correct[track_id]);
	}
	else 
	{
		// average incresement of unscaled pts
		_pts_avg_inc[track_id] = media_packet->GetPts() - _stat_recv_pkt_lpts[track_id];
	}
	
	_stat_recv_pkt_lpts[track_id] = media_packet->GetPts();
	_stat_recv_pkt_size[track_id] += media_packet->GetData()->GetLength();
	_stat_recv_pkt_count[track_id] ++;

	// Set the corrected PTS.
	media_packet->SetPts( media_packet->GetPts() - _pts_correct[track_id] );
	media_packet->SetDts( media_packet->GetDts() - _pts_correct[track_id] );

	// for statistics...s
	time_t curr_time;
	time(&curr_time);

	if(difftime(curr_time, _last_recv_time) >= 5)
	{
		ov::String temp_str = "\n";

		time_t uptime = curr_time-_stat_start_time;
		temp_str.AppendFormat("stats of stream[%s] uptime : %llds , queue : %d\n" ,_stream->GetName().CStr(), uptime, _media_packets.size());
		for(const auto &iter : _stream->GetTracks())
		{
			auto track_id = iter.first;
			auto track = iter.second;

			temp_str.AppendFormat("\t[%d] media : %s, crt : %lld, last_ts : %lldms->%lldms(%lld->%lld), rcv_cnt : %lld, rcv_siz : %lldb\n"
				, track_id
				, track->GetMediaType()==MediaType::Video?"video":"audio"
				, _pts_correct[track_id]
				, _stat_recv_pkt_lpts[track_id] * 1000 / track->GetTimeBase().GetDen()
				, (_stat_recv_pkt_lpts[track_id] - _pts_correct[track_id])  * 1000 / track->GetTimeBase().GetDen()
				, _stat_recv_pkt_lpts[track_id]
				, _stat_recv_pkt_lpts[track_id] - _pts_correct[track_id]
				, _stat_recv_pkt_count[track_id]
				, _stat_recv_pkt_size[track_id]);
		}

		logtd("%s", temp_str.CStr());

		_last_recv_time = curr_time;
	}



	if (media_type == MediaType::Video)
	{
		if (media_track->GetCodecId() == MediaCodecId::H264)
		{
			// If there is no RTP fragmentation, Create!
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
	return _last_recv_time;
}
