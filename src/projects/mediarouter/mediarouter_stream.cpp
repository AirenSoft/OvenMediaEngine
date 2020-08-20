//==============================================================================
//
//  MediaRouteStream
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "mediarouter_stream.h"

#include <base/ovlibrary/ovlibrary.h>

#include <modules/codec_analyzer/h264/h264_decoder_configuration_record.h>
#include <modules/codec_analyzer/h264/h264_sps.h>
#include <modules/codec_analyzer/h264/h264_fragment_header.h>
#include <modules/codec_analyzer/h264/h264_avcc_to_annexb.h>

#include <modules/codec_analyzer/aac/aac_specific_config.h>
#include <modules/codec_analyzer/aac/aac_adts.h>
#include <modules/codec_analyzer/aac/aac_latm_to_adts.h>

#define OV_LOG_TAG "MediaRouter.Stream"
#define PTS_CORRECT_THRESHOLD_US	5000

using namespace common;

MediaRouteStream::MediaRouteStream(const std::shared_ptr<info::Stream> &stream) :
	_created_stream(false),
	_stream(stream),
	_packets_queue(nullptr, 100)
{
	logti("Trying to create media route stream: name(%s) id(%u)", stream->GetName().CStr(), stream->GetId());
	_inout_type = MRStreamInoutType::Unknown; 

	_stat_start_time = std::chrono::system_clock::now();

	_stop_watch.Start();

	_packets_queue.SetAlias(ov::String::FormatString("%s/%s - MediaRouterStream packets Queue", _stream->GetApplicationInfo().GetName().CStr() ,_stream->GetName().CStr()));
}

MediaRouteStream::MediaRouteStream(const std::shared_ptr<info::Stream> &stream, MRStreamInoutType inout_type) :
	MediaRouteStream(stream)
{
	_inout_type = inout_type; 
}

MediaRouteStream::~MediaRouteStream()
{
	logti("Delete media route stream name(%s) id(%u)", _stream->GetName().CStr(), _stream->GetId());

	_media_packet_stored.clear();

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

void MediaRouteStream::SetInoutType(MRStreamInoutType inout_type)
{
	_inout_type = inout_type;
}

MRStreamInoutType MediaRouteStream::GetInoutType()
{
	return _inout_type;
}

bool MediaRouteStream::IsValidStream()
{
	return true;
}

bool MediaRouteStream::IsCreatedSteam()
{
	return _created_stream;
}

void MediaRouteStream::SetCreatedSteam(bool created)
{
	_created_stream = created;
}
	

bool MediaRouteStream::PushIncomingStream(std::shared_ptr<MediaPacket> &media_packet)
{
	auto media_track = _stream->GetTrack(media_packet->GetTrackId());
	if (media_track == nullptr)
	{
		logte("Not found mediatrack. id(%d)", media_packet->GetTrackId());
		return false;
	}	

	////////////////////////////////////////////////////
	// Bitstream converting to standard format
	// 
	// Standard format by codec
	// 	 - h264 : H264_ANNEXB
	// 	 - aac  : AAC_ADTS
	// 	 - vp8  : VP8
	// 	 - opus : OPUS   <CELT | SILK>
	//////////////////////////////////////////////////////
	switch(media_track->GetCodecId())
	{
		case MediaCodecId::H264:
			if(media_packet->GetBitstreamFormat() == common::BitstreamFormat::H264_AVCC)
			{
				std::vector<uint8_t> extradata; 
				if( H264AvccToAnnexB::GetExtradata(media_packet->GetPacketType(), media_packet->GetData(), extradata) == true)
				{
					media_track->SetCodecExtradata(extradata);
					return false;
				}

				if(H264AvccToAnnexB::Convert(media_packet, media_track->GetCodecExtradata()) == false)
				{
					logte("Bitstream format change failed");
					return false;
				}
			}
			break;	

		case MediaCodecId::Aac:
			if(media_packet->GetBitstreamFormat() == common::BitstreamFormat::AAC_LATM)
			{
				
				std::vector<uint8_t> extradata; 
				if( AACLatmToAdts::GetExtradata(media_packet->GetPacketType(), media_packet->GetData(), extradata) == true)
				{
					media_track->SetCodecExtradata(extradata);
					return false;
				}
				
				if(AACLatmToAdts::Convert(media_packet, media_track->GetCodecExtradata()) == false)
				{
					logte("Bitstream format change failed");
					return false;
				}
			}
			break;
		
		// The incoming stream does not support this codec.
		case MediaCodecId::Vp8: 
		case MediaCodecId::Vp9:
		case MediaCodecId::Flv:
		case MediaCodecId::Opus:
		case MediaCodecId::Mp3:
			logte("Not support codec in incoming stream");
			return false;

		default:
			logte("Unknown codec");
			return false;
	}

	////////////////////////////////////////////////////
	// Codec Analyzer
	// 
	// Extract codec information from a bit stream.
	////////////////////////////////////////////////////
	switch(media_track->GetCodecId())
	{
		case MediaCodecId::H264:
			if(media_packet->GetBitstreamFormat() == common::BitstreamFormat::H264_ANNEXB)
			{
				// NALU 패킷을 분석해서 SPS/PPS 타입에 대해서 트랙 정보를 추출한다.		
				if(H264FragmentHeader::Parse(media_packet) == false)
				{
					logte("failed make fragment header");
				}

				auto payload_data = media_packet->GetData()->GetDataAs<uint8_t>();
				auto fragmentation = media_packet->GetFragHeader();

				for (size_t i = 0; i < fragmentation->GetCount(); ++i) 
				{
					const uint8_t* buffer = &payload_data[fragmentation->fragmentation_offset[i]];
					size_t length = fragmentation->fragmentation_length[i];

					uint16_t nal_unit_type = *(buffer) & 0x1F;

					if(nal_unit_type == 7)
					{
						H264Sps sps;
						H264Sps::Parse(buffer, length, sps);
//						logtd("%s", sps.GetInfoString().CStr());
					}
				}
			}
			break;

		case MediaCodecId::Aac:
			if(media_packet->GetBitstreamFormat() == common::BitstreamFormat::AAC_ADTS)
			{
				if(AACAdts::IsValid(media_packet->GetData()->GetDataAs<uint8_t>(), media_packet->GetDataLength()) == true)
				{
					AACAdts adts;
					AACAdts::Parse(media_packet->GetData()->GetDataAs<uint8_t>(), media_packet->GetDataLength(), adts);
					
					// logtd("%s", adts.GetInfoString().CStr());
				}
			}		
			break;

		// The incoming stream does not support this codec.
		case MediaCodecId::Vp8: 
		case MediaCodecId::Opus:
		case MediaCodecId::Vp9:
		case MediaCodecId::Flv:
		case MediaCodecId::Mp3:
			logte("Not support codec in incoming stream");
			break;

		default:
			logte("Unknown codec");
			break;		
	}

	return true;	
}

bool MediaRouteStream::PushOutgoungStream(std::shared_ptr<MediaPacket> &media_packet)
{

	return true;
}

bool MediaRouteStream::Push(std::shared_ptr<MediaPacket> media_packet)
{	
	// logtd("track_id(%d), type(%d), bsfmt(%d), flags(%d)", media_packet->GetTrackId(), media_packet->GetPacketType(), media_packet->GetBitstreamFormat(), media_packet->GetFlag());
	// logtd("%s", media_packet->GetData()->Dump(128).CStr());		

	// Bitstream format converting to stand format. and, parsing track informaion
	if(GetInoutType() == MRStreamInoutType::Incoming)
	{
		if(!PushIncomingStream(media_packet))
			return false;
	}
	else if(GetInoutType() == MRStreamInoutType::Outgoing)
	{
		if(!PushOutgoungStream(media_packet))
			return false;
	}

	// Accumulate Packet duplication
	//	- 1) If packets stored in temporary storage exist, calculate Duration compared to the current packet's timestamp.
	//	- 2) If the current packet does not have a Duration value, keep it in a temporary store.
	//	- 3) If there is a packet Duration value, insert it into the packet queue.
	bool is_inserted_queue = false;

	auto iter = _media_packet_stored.find(media_packet->GetTrackId());
	if(iter != _media_packet_stored.end())
	{
		auto media_packet_cache = iter->second;
		_media_packet_stored.erase(iter);

		int64_t duration = media_packet->GetDts() - media_packet_cache->GetDts();
		media_packet_cache->SetDuration(duration);

		_packets_queue.Enqueue(std::move(media_packet_cache));

		is_inserted_queue = true;
	}

	if(media_packet->GetDuration() == -1LL)
	{
		_media_packet_stored[media_packet->GetTrackId()] = media_packet;	
	}
	else
	{
		_packets_queue.Enqueue(std::move(media_packet));

		is_inserted_queue = true;
	}

	return is_inserted_queue;
}


std::shared_ptr<MediaPacket> MediaRouteStream::Pop()
{
	if(_packets_queue.IsEmpty())
	{
		return nullptr;
	}

	auto media_packet_ref = _packets_queue.Dequeue();
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
		// TODO(soulk): I think all tracks should calibrate the PTS with the same value.
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
			, _inout_type==MRStreamInoutType::Incoming?"Incoming":"Outgoing"
			,_stream->GetApplicationInfo().GetName().CStr()
			,_stream->GetName().CStr()
			,(int64_t)uptime, _packets_queue.Size());

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

				pts_str.AppendFormat("last_pts: %lldms -> %lldms, crt_pts: %lld, delay: %5lldms"
					, rescaled_last_pts
					, rescaled_last_pts - corrected_pts
					, corrected_pts
					, first_delay - last_delay
					 );
			}
			else
			{
				pts_str.AppendFormat("last_pts: %lldms, delay: %5lldms"
					, rescaled_last_pts
					, first_delay - last_delay
				);
			}

			temp_str.AppendFormat("\n\t[%d] track: %s(%d), %s, pkt_cnt: %6lld, pkt_siz: %sB"
				, track_id
				, track->GetMediaType()==MediaType::Video?"video":"audio"
				, track->GetCodecId()
				, pts_str.CStr()
				, _stat_recv_pkt_count[track_id]
				, ov::Converter::ToSiString(_stat_recv_pkt_size[track_id], 1).CStr()
			);
		}

		logtd("%s", temp_str.CStr());
	}

	switch(media_track->GetCodecId())
	{
		case MediaCodecId::H264:
			// NALU 패킷을 분석해서 SPS/PPS 타입에 대해서 트랙 정보를 추출한다.		
			if(H264FragmentHeader::Parse(media_packet) == false)
			{
				logte("failed make fragment header");
			}
		break;		
		default:
		break;
	}
	// Set the corrected PTS.
	media_packet->SetPts( media_packet->GetPts() - _pts_correct[track_id] );
	media_packet->SetDts( media_packet->GetDts() - _pts_correct[track_id] );

	return media_packet;
}
