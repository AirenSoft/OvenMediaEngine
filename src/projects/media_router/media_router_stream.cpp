//==============================================================================
//
//  MediaRouteStream
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "media_router_stream.h"

#include <base/ovlibrary/ovlibrary.h>

#define OV_LOG_TAG "MediaRouter.Stream"

using namespace common;

#define PTS_CORRECT_THRESHOLD_US	5000	

MediaRouteStream::MediaRouteStream(const std::shared_ptr<info::Stream> &stream)
{
	logtd("Trying to create media route stream: name(%s) id(%u)", stream->GetName().CStr(), stream->GetId());

	_inout_type = false;
	_stream = stream;
	_stream->ShowInfo();

	_stat_start_time = std::chrono::system_clock::now();

	_stop_watch.Start();


	// set alias
	_media_packets.SetAlias(ov::String::FormatString("%s/%s - Mediarouter stream a/v queue", _stream->GetApplicationInfo().GetName().CStr() ,_stream->GetName().CStr()));
}

MediaRouteStream::~MediaRouteStream()
{
	logtd("Delete media route stream name(%s) id(%u)", _stream->GetName().CStr(), _stream->GetId());

	_media_packet_stored.clear();

	_media_packets.Clear();

	_stat_recv_pkt_lpts.clear();
	_stat_recv_pkt_ldts.clear();
	_stat_recv_pkt_size.clear();
	_stat_recv_pkt_count.clear();
	_stat_first_time_diff.clear();

	_pts_correct.clear();
	_pts_avg_inc.clear();
}

std::shared_ptr<info::Stream> MediaRouteStream::GetStream()
{
	return _stream;
}

void MediaRouteStream::SetConnectorType(MediaRouteApplicationConnector::ConnectorType type)
{
	_application_connector_type = type;
}

void MediaRouteStream::SetInoutType(bool inout_type)
{
	_inout_type = inout_type;
}


MediaRouteApplicationConnector::ConnectorType MediaRouteStream::GetConnectorType()
{
	return _application_connector_type;
}

bool MediaRouteStream::Push(std::shared_ptr<MediaPacket> media_packet)
{	
	auto track_id = media_packet->GetTrackId();

	// Accumulate Packet duplication
	//	- 1) If packets stored in temporary storage exist, calculate Duration compared to the current packet's timestamp.
	//	- 2) If the current packet does not have a Duration value, keep it in a temporary store.
	//	- 3) If there is a packet Duration value, insert it into the packet queue.
	bool is_inserted_queue = false;

	auto iter = _media_packet_stored.find(track_id);
	if(iter != _media_packet_stored.end())
	{
		auto media_packet_cache = iter->second;
		_media_packet_stored.erase(iter);

		int64_t duration = media_packet->GetDts() - media_packet_cache->GetDts();
		media_packet_cache->SetDuration(duration);

		_media_packets.Enqueue(std::move(media_packet_cache));
		is_inserted_queue = true;
	}

	if(media_packet->GetDuration() == -1LL)
	{
		_media_packet_stored[track_id] = media_packet;	
	}
	else
	{
		_media_packets.Enqueue(std::move(media_packet));

		is_inserted_queue = true;
	}

	return is_inserted_queue;
}


std::shared_ptr<MediaPacket> MediaRouteStream::Pop()
{
	if(_media_packets.IsEmpty())
	{
		return nullptr;
	}

	auto media_packet_ref = _media_packets.Dequeue();
	if(media_packet_ref.has_value() == false)
	{
		return nullptr;
	}	

	auto &media_packet = media_packet_ref.value();

	auto media_type = media_packet->GetMediaType();
	auto track_id = media_packet->GetTrackId();
	auto media_track = _stream->GetTrack(track_id);

	if (media_track == nullptr)
	{
		logte("Cannot find media track. media_type(%s), track_id(%d)", (media_type == MediaType::Video) ? "video" : "audio", media_packet->GetTrackId());
		return nullptr;
	}

	////////////////////////////////////////////////////////////////////////////////////
	// PTS Correction for Abnormal increase
	////////////////////////////////////////////////////////////////////////////////////
	
	int64_t timestamp_delta = media_packet->GetPts()  - _stat_recv_pkt_lpts[track_id];
	int64_t scaled_timestamp_delta = timestamp_delta * 1000 /  media_track->GetTimeBase().GetDen();

	if (abs( scaled_timestamp_delta ) > PTS_CORRECT_THRESHOLD_US )
	{
		_pts_correct[track_id] = media_packet->GetPts() - _stat_recv_pkt_lpts[track_id] - _pts_avg_inc[track_id];

#if 0
		logtw("Detected abnormal increased pts. track_id : %d, prv_pts : %lld, cur_pts : %lld, crt_pts : %lld, avg_inc : %lld"
			, track_id
			, _stat_recv_pkt_lpts[track_id]
			, media_packet->GetPts()
			, _pts_correct[track_id]
			, _pts_avg_inc[track_id]
		);
#endif
	}
	else
	{
		// Originally it should be an average value, Use the difference of the last packet.
		// Use DTS because the PTS value does not increase uniformly.
		_pts_avg_inc[track_id] = media_packet->GetDts() - _stat_recv_pkt_ldts[track_id];
	}
	

	////////////////////////////////////////////////////////////////////////////////////
	// Statistics for log
	////////////////////////////////////////////////////////////////////////////////////
	_stat_recv_pkt_lpts[track_id] = media_packet->GetPts();

	_stat_recv_pkt_ldts[track_id] = media_packet->GetDts();
	
	_stat_recv_pkt_size[track_id] += media_packet->GetData()->GetLength();
	
	_stat_recv_pkt_count[track_id]++;


	// 	Diffrence time of received first packet with uptime.
	if(_stat_first_time_diff[track_id] == 0)
	{
		auto curr_time = std::chrono::system_clock::now();
		int64_t uptime =  std::chrono::duration_cast<std::chrono::milliseconds>(curr_time - _stat_start_time).count();

		int64_t rescaled_last_pts = _stat_recv_pkt_lpts[track_id] * 1000 /_stream-> GetTrack(track_id)->GetTimeBase().GetDen();

		_stat_first_time_diff[track_id] = uptime - rescaled_last_pts;
	}

	if (_stop_watch.IsElapsed(5000))
	{
		_stop_watch.Update();

		auto curr_time = std::chrono::system_clock::now();

		// Uptime
		int64_t uptime =  std::chrono::duration_cast<std::chrono::milliseconds>(curr_time - _stat_start_time).count();

		ov::String temp_str = "\n";
		temp_str.AppendFormat(" - Stream of MediaRouter| type: %s, name: %s/%s, uptime: %lldms , queue: %d" 
			, _inout_type?"Outgoing":"Incoming"
			,_stream->GetApplicationInfo().GetName().CStr()
			,_stream->GetName().CStr()
			,(int64_t)uptime, _media_packets.Size());

		for(const auto &iter : _stream->GetTracks())
		{
			auto track_id = iter.first;
			auto track = iter.second;

			ov::String pts_str = "";

			// 1/1000 초 단위로 PTS 값을 변환
			int64_t rescaled_last_pts = _stat_recv_pkt_lpts[track_id] * 1000 / track->GetTimeBase().GetDen();

			// 최소 패킷이 들어오는 시간
			int64_t first_delay = _stat_first_time_diff[track_id];

			int64_t last_delay = uptime-rescaled_last_pts;

			if(_pts_correct[track_id] != 0)
			{
				int64_t corrected_pts = _pts_correct[track_id] * 1000 / track->GetTimeBase().GetDen();

				pts_str.AppendFormat("last_pts(%lldms->%lldms), fist_diff(%5lldms), last_diff(%5lldms), delay(%5lldms), crt_pts : %lld"
					, rescaled_last_pts
					, rescaled_last_pts - corrected_pts
					, first_delay
					, last_delay
					, first_delay - last_delay
					, corrected_pts );
			}
			else
			{
				pts_str.AppendFormat("last_pts(%lldms), fist_diff(%5lldms), last_diff(%5lldms), delay(%5lldms)"
					, rescaled_last_pts
					, first_delay
					, last_delay
					, first_delay - last_delay
				);
			}

			temp_str.AppendFormat("\n\t[%d] track: %s(%d), %s, pkt_cnt: %lld, pkt_siz: %lldB"
				, track_id
				, track->GetMediaType()==MediaType::Video?"video":"audio"
				, track->GetCodecId()
				, pts_str.CStr()
				, _stat_recv_pkt_count[track_id]
				, _stat_recv_pkt_size[track_id]);
		}

		logtd("%s", temp_str.CStr());
	}


	////////////////////////////////////////////////////////////////////////////////////
	// Bitstream Processing
	////////////////////////////////////////////////////////////////////////////////////

	// Bitstream Converting or Generate Fragmentation Header 
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
			_bsf_vp8.convert_to(media_packet->GetData());
		}
		else
		{
			logte("Unsupported video codec. codec_id(%d)", media_track->GetCodecId());
			return nullptr;
		}
	}
	else if (media_type == MediaType::Audio)
	{
		if (media_track->GetCodecId() == MediaCodecId::Aac)
		{
			// _bsfa.convert_to(media_packet->GetData());
			// logtd("%s", media_packet->GetData()->Dump(32).CStr());
		}
		else if (media_track->GetCodecId() == MediaCodecId::Opus)
		{

		}
		else
		{
			logte("Unsupported audio codec. codec_id(%d)", media_track->GetCodecId());
			return nullptr;
		}
	}
	else
	{
		logte("Unsupported media typec. media_type(%d)", media_type);
		return nullptr;
	}

	// Set the corrected PTS.
	media_packet->SetPts( media_packet->GetPts() - _pts_correct[track_id] );
	media_packet->SetDts( media_packet->GetDts() - _pts_correct[track_id] );

	return media_packet;
}
