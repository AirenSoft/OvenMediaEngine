//==============================================================================
//
//  MediaRouteStream
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

// Role Definition 
// TODO(soulk) - Describes the features supported by the media router.

#include "mediarouter_stream.h"

#include <base/ovlibrary/ovlibrary.h>

#include <modules/bitstream/h264/h264_decoder_configuration_record.h>
#include <modules/bitstream/h264/h264_parser.h>
#include <modules/bitstream/h264/h264_avcc_to_annexb.h>
#include <modules/bitstream/h264/h264_nal_unit_types.h>

#include <modules/bitstream/h265/h265_parser.h>

#include <modules/bitstream/aac/aac_specific_config.h>
#include <modules/bitstream/aac/aac_adts.h>
#include <modules/bitstream/aac/aac_latm_to_adts.h>

#include <modules/bitstream/nalu/nal_unit_fragment_header.h>

#define OV_LOG_TAG "MediaRouter.Stream"
#define PTS_CORRECT_THRESHOLD_US	5000

using namespace common;

MediaRouteStream::MediaRouteStream(const std::shared_ptr<info::Stream> &stream) :
	_is_parsed_all_track(false),
	_is_created_stream(false),
	_stream(stream),
	_packets_queue(nullptr, 100)
{
	logti("Trying to create media route stream: name(%s) id(%u)", stream->GetName().CStr(), stream->GetId());
	_inout_type = MRStreamInoutType::Unknown; 

	_stat_start_time = std::chrono::system_clock::now();

	_stop_watch.Start();

	_packets_queue.SetAlias(ov::String::FormatString("%s/%s - MediaRouterStream packets Queue", _stream->GetApplicationInfo().GetName().CStr() ,_stream->GetName().CStr()));

	InitParseTrackInfo();
}

MediaRouteStream::MediaRouteStream(const std::shared_ptr<info::Stream> &stream, MRStreamInoutType inout_type) :
	MediaRouteStream(stream)
{
	_inout_type = inout_type; 
}

MediaRouteStream::~MediaRouteStream()
{
	logti("Delete media route stream name(%s) id(%u)", _stream->GetName().CStr(), _stream->GetId());

	_media_packet_stash.clear();

	_stat_recv_pkt_lpts.clear();
	_stat_recv_pkt_ldts.clear();
	_stat_recv_pkt_size.clear();
	_stat_recv_pkt_count.clear();
	_stat_first_time_diff.clear();

	_pts_last.clear();
	_dts_last.clear();
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


// Check if an external stream has been generated for the current stream.
bool MediaRouteStream::IsCreatedSteam()
{
	return _is_created_stream;
}

void MediaRouteStream::SetCreatedSteam(bool created)
{
	_is_created_stream = created;
}

bool MediaRouteStream::PushIncomingStream(std::shared_ptr<MediaTrack> &media_track, std::shared_ptr<MediaPacket> &media_packet)
{
	// Convert to default bitstream format
	if(!ConvertToDefaultBitstream(media_track, media_packet))
	{
		return false;
	}

	// Parse Addional Data. fragment header, keyframe etc... 
	if(!ParseAdditionalData(media_track, media_packet))
	{
		return false;
	}

	// Extract media track information
	if(!ParseTrackInfo(media_track, media_packet))
	{
		return false;
	}


	return true;	
}

bool MediaRouteStream::PushOutgoungStream(std::shared_ptr<MediaTrack> &media_track, std::shared_ptr<MediaPacket> &media_packet)
{
	if(!ParseAdditionalData(media_track, media_packet))
	{
		return false;
	}
	
	return true;
}

void MediaRouteStream::InitParseTrackInfo()
{
	for(const auto &iter : _stream->GetTracks())
	{
		auto track_id = iter.first;
		auto track = iter.second;

		_parse_completed_track_info[track_id] = false;
		_incoming_tiembase[track_id] = track->GetTimeBase();
	}
}

void MediaRouteStream::SetParseTrackInfo(std::shared_ptr<MediaTrack> &media_track, bool parsed)
{
	_parse_completed_track_info[media_track->GetId()] = parsed;
}


bool MediaRouteStream::GetParseTrackInfo(std::shared_ptr<MediaTrack> &media_track)
{
	auto iter = _parse_completed_track_info.find(media_track->GetId());
	if(iter != _parse_completed_track_info.end())
	{
		return iter->second;
	}

	return false;
}

// Check whether the information extraction for all tracks has been completed.
bool MediaRouteStream::IsParseTrackAll()
{
	// If the parsing is already done...
	if(_is_parsed_all_track)
	{
		return true;
	}

	for(const auto &iter : _parse_completed_track_info)
	{
		if(iter.second == false)
			return false;
	}

	_is_parsed_all_track = true;

	return _is_parsed_all_track;
}

// Extract codec information from a media packet
bool MediaRouteStream::ParseTrackInfo(std::shared_ptr<MediaTrack> &media_track, std::shared_ptr<MediaPacket> &media_packet)
{
	if(IsParseTrackAll())
	{
		return true;	
	}

	// Check if parsing is already finished.
	if(GetParseTrackInfo(media_track) == true)
	{
		return true;
	}

	// Parse media track information by codec. 
	switch(media_track->GetCodecId())
	{
		case MediaCodecId::H264:
			if(media_packet->GetBitstreamFormat() == common::BitstreamFormat::H264_ANNEXB)
			{
				// Analyzes NALU packets and extracts track information for SPS/PPS types.
				auto payload_data = media_packet->GetData()->GetDataAs<uint8_t>();
				auto frag_hdr = media_packet->GetFragHeader();
				if(frag_hdr == nullptr)
				{
					logte("Could not find fragment header");
					return false;
				}

				for (size_t i = 0; i < frag_hdr->GetCount(); ++i) 
				{
					const uint8_t* buffer = &payload_data[frag_hdr->fragmentation_offset[i]];
					size_t length = frag_hdr->fragmentation_length[i];

					H264NalUnitHeader header;
					if(H264Parser::ParseNalUnitHeader(buffer, length, header) == false)
					{
						logte("Could not parse H264 Nal unit header");
						return false;
					}
					
					if(header.GetNalUnitType() == H264NalUnitType::Sps)
					{
						H264SPS sps;
						if(H264Parser::ParseSPS(buffer, length, sps) == false)
						{
							logte("Could not parse H264 SPS unit");
							return false;

						}

						media_track->SetWidth(sps.GetWidth());
						media_track->SetHeight(sps.GetHeight());
						media_track->SetTimeBase(1, 90000);

						logtd("%s", sps.GetInfoString().CStr());
						logtd("[%d] Convert timebase (%d/%d) to (%d/%d)"
							, media_track->GetId()
							, _incoming_tiembase[media_track->GetId()].GetNum(), _incoming_tiembase[media_track->GetId()].GetDen()
							, media_track->GetTimeBase().GetNum(), media_track->GetTimeBase().GetDen());

						SetParseTrackInfo(media_track, true);
					}
				}
			}
			break;

		case MediaCodecId::H265:
			if(media_packet->GetBitstreamFormat() == common::BitstreamFormat::H265_ANNEXB)
			{
				// Analyzes NALU packets and extracts track information for SPS/PPS types.
				auto payload_data = media_packet->GetData()->GetDataAs<uint8_t>();
				auto frag_hdr = media_packet->GetFragHeader();
				if(frag_hdr == nullptr)
				{
					logte("Could not find fragment header");
					return false;
				}
				
				for (size_t i = 0; i < frag_hdr->GetCount(); ++i) 
				{
					const uint8_t* buffer = &payload_data[frag_hdr->fragmentation_offset[i]];
					size_t length = frag_hdr->fragmentation_length[i];

					H265NalUnitHeader header;
					if(H265Parser::ParseNalUnitHeader(buffer, length, header) == false)
					{
						logte("Could not parse H265 Nal unit header");
						return false;
					}

					if(header.GetNalUnitType() == H265NALUnitType::SPS)
					{
						H265SPS sps;
						if(H265Parser::ParseSPS(buffer, length, sps) == false)
						{
							logte("Could not parse H265 SPS Unit");
							return false;
						}

						media_track->SetWidth(sps.GetWidth());
						media_track->SetHeight(sps.GetHeight());
						media_track->SetTimeBase(1, 90000);

						logtd("%s", sps.GetInfoString().CStr());					
						logtd("[%d] Convert timebase (%d/%d) to (%d/%d)"
							, media_track->GetId()
							, _incoming_tiembase[media_track->GetId()].GetNum(), _incoming_tiembase[media_track->GetId()].GetDen()
							, media_track->GetTimeBase().GetNum(), media_track->GetTimeBase().GetDen());

						SetParseTrackInfo(media_track, true);
					}
				}

			} break;
		
		case MediaCodecId::Aac:
			if(media_packet->GetBitstreamFormat() == common::BitstreamFormat::AAC_ADTS)
			{
				if(AACAdts::IsValid(media_packet->GetData()->GetDataAs<uint8_t>(), media_packet->GetDataLength()) == false)
				{
					logte("Could not parse AAC ADTS header");
					return false;
				}

				AACAdts adts;
				if(AACAdts::Parse(media_packet->GetData()->GetDataAs<uint8_t>(), media_packet->GetDataLength(), adts) == true)
				{					
					media_track->SetSampleRate(adts.SamplerateNum());
					media_track->GetChannel().SetLayout( (adts.SamplerateNum()==1)?(AudioChannel::Layout::LayoutMono):(AudioChannel::Layout::LayoutStereo) );
					media_track->SetTimeBase(1, media_track->GetSampleRate());


					logtd("%s", adts.GetInfoString().CStr());
					logtd("[%d] Convert timebase(%d/%d) -> timebase(%d/%d)"
						, media_track->GetId()
						, _incoming_tiembase[media_track->GetId()].GetNum(), _incoming_tiembase[media_track->GetId()].GetDen()
						, media_track->GetTimeBase().GetNum(), media_track->GetTimeBase().GetDen());

					if(media_packet->GetDataLength() - adts.AacFrameLength() != 0)
						logte("Invalid frame length. adts length : %d != packet length : %d", adts.AacFrameLength(), media_packet->GetDataLength());

					SetParseTrackInfo(media_track, true);
				}

			}		
			break;

		// The incoming stream does not support this codec.
		case MediaCodecId::Vp8: 
		case MediaCodecId::Vp9:
		case MediaCodecId::Opus:
			logte("Not supported a codec for parsing");
			break;
		default:
			logte("Unknown codec");
			break;		
	}

	return true;
}

// Parse fragment header, flags etc...
bool MediaRouteStream::ParseAdditionalData(
	std::shared_ptr<MediaTrack> &media_track, 
	std::shared_ptr<MediaPacket> &media_packet)
{
	// Fragment Header
	switch(media_track->GetCodecId())
	{
		case MediaCodecId::H264:
		{
			if(media_packet->GetFragHeader()->GetCount() == 0)
			{
				NalUnitFragmentHeader nal_parser;
				if(NalUnitFragmentHeader::Parse(media_packet->GetData(), nal_parser) == false)
				{
					logte("failed to parse fragment header");
					return false;
				}

				media_packet->SetFragHeader(nal_parser.GetFragmentHeader());
			}

			if(H264Parser::CheckKeyframe(media_packet->GetData()->GetDataAs<uint8_t>(), media_packet->GetData()->GetLength()) == true)
			{
				media_packet->SetFlag(MediaPacketFlag::Key);
			}

		} break;		
		case MediaCodecId::H265:
		{
			if(media_packet->GetFragHeader()->GetCount() == 0)
			{
				NalUnitFragmentHeader nal_parser;
				if(NalUnitFragmentHeader::Parse(media_packet->GetData(), nal_parser) == false)
				{
					logte("failed to parse fragment header");
					return false;
				}

				media_packet->SetFragHeader(nal_parser.GetFragmentHeader());
			}

			if(H265Parser::CheckKeyframe(media_packet->GetData()->GetDataAs<uint8_t>(), media_packet->GetData()->GetLength()) == true)
			{
				media_packet->SetFlag(MediaPacketFlag::Key);
			}
			
		} break;

		case MediaCodecId::Aac: 
		case MediaCodecId::Vp8: 
		case MediaCodecId::Vp9:
		case MediaCodecId::Opus:
		default:
			media_packet->SetFlag(MediaPacketFlag::Key);
		break;
	}


	return true;
}

// Bitstream converting to standard format
// 
// Standard format by codec
// 	 - h264 : H264_ANNEXB
// 	 - aac  : AAC_ADTS
// 	 - vp8  : VP8
// 	 - opus : OPUS   <CELT | SILK>
bool MediaRouteStream::ConvertToDefaultBitstream(std::shared_ptr<MediaTrack> &media_track, std::shared_ptr<MediaPacket> &media_packet)
{
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

				if(H264AvccToAnnexB::Convert(media_packet->GetPacketType(), media_packet->GetData(), media_track->GetCodecExtradata()) == false)
				{
					logte("Failed to change bitstream format ");
					return false;
				}
				
				media_packet->SetBitstreamFormat(common::BitstreamFormat::H264_ANNEXB);
				media_packet->SetPacketType(common::PacketType::NALU);
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
				
				if(AACLatmToAdts::Convert(media_packet->GetPacketType(), media_packet->GetData(), media_track->GetCodecExtradata()) == false)
				{
					logte("Failed to change bitstream format");
					return false;
				}
			
				media_packet->SetBitstreamFormat(common::BitstreamFormat::AAC_ADTS);
				media_packet->SetPacketType(common::PacketType::RAW);
			}
			break;
		// The incoming stream does not support this codec.
		case MediaCodecId::H265:
			if(media_packet->GetBitstreamFormat() != common::BitstreamFormat::H265_ANNEXB)
			{
				logte("Invalid H265 bitstream format");
				return false;
			}
			return true;
		case MediaCodecId::Vp8: 
		case MediaCodecId::Vp9:
		case MediaCodecId::Opus:
			logte("Not support codec in incoming stream");
			return false;

		default:
			logte("Unknown codec");
			return false;
	}

	return true;	
}

bool MediaRouteStream::ConvertToDefaultTimestamp(
	std::shared_ptr<MediaTrack> &media_track,
	std::shared_ptr<MediaPacket> &media_packet)
{

	// If the timebase is the same, there is no need to change the timestamp.
	if(_incoming_tiembase[media_track->GetId()] == media_track->GetTimeBase())
	{
		return true;
	}

	// TODO(soulk) : Need to performacne tuning
	// 				 Reduce calculating
	double scale = _incoming_tiembase[media_track->GetId()].GetExpr() / media_track->GetTimeBase().GetExpr();

	media_packet->SetPts( media_packet->GetPts() * scale );
	media_packet->SetDts( media_packet->GetDts() * scale );
	media_packet->SetDuration( media_packet->GetDuration() * scale );

	return true;
}


void MediaRouteStream::UpdateStatistics(std::shared_ptr<MediaTrack> &media_track, std::shared_ptr<MediaPacket> &media_packet)
{
	auto track_id = media_track->GetId();

	_stat_recv_pkt_lpts[track_id] = media_packet->GetPts();

	_stat_recv_pkt_ldts[track_id] = media_packet->GetDts();
	
	_stat_recv_pkt_size[track_id] += media_packet->GetData()->GetLength();

	_stat_recv_pkt_count[track_id]++;

	// 	Diffrence time of received first packet with uptime.
	if(_stat_first_time_diff[track_id] == 0)
	{
		int64_t uptime =  std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - _stat_start_time).count();

		int64_t rescaled_last_pts = _stat_recv_pkt_lpts[track_id] * 1000 /_stream-> GetTrack(track_id)->GetTimeBase().GetDen();

		_stat_first_time_diff[track_id] = uptime - rescaled_last_pts;
	}

	if (_stop_watch.IsElapsed(5000))
	{
		_stop_watch.Update();

		// Uptime
		int64_t uptime =  std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - _stat_start_time).count();

		int64_t min_pts = -1LL;
		int64_t max_pts = -1LL;

		ov::String stat_track_str = "";

		for(const auto &iter : _stream->GetTracks())
		{
			auto track_id = iter.first;
			auto track = iter.second;

			ov::String pts_str = "";

			int64_t rescaled_last_pts = _stat_recv_pkt_lpts[track_id] * 1000 / track->GetTimeBase().GetDen();

			int64_t first_delay = _stat_first_time_diff[track_id];

			int64_t last_delay = uptime-rescaled_last_pts;

			if(_pts_correct[track_id] != 0)
			{
				int64_t corrected_pts = _pts_correct[track_id] * 1000 / track->GetTimeBase().GetDen();

				pts_str.AppendFormat("pts: %lldms, crt: %lld, delay: %5lldms"
					, rescaled_last_pts
					, corrected_pts
					, first_delay - last_delay
					 );
			}
			else
			{
				pts_str.AppendFormat("pts: %lldms, delay: %5lldms"
					, rescaled_last_pts
					, first_delay - last_delay
				);
			}

			// calc min/max pts
			if(min_pts == -1LL)
				min_pts = rescaled_last_pts;
			if(max_pts == -1LL)
				max_pts = rescaled_last_pts;
			
			min_pts = std::min(min_pts, rescaled_last_pts);
			max_pts = std::max(max_pts, rescaled_last_pts);

			stat_track_str.AppendFormat("\n\t[%3d] type: %5s(%2d/%4s), %s, pkt_cnt: %6lld, pkt_siz: %sB"
				, track_id
				, track->GetMediaType()==MediaType::Video?"video":"audio"
				, track->GetCodecId()
				, ov::Converter::ToString(track->GetCodecId()).CStr()
				, pts_str.CStr()
				, _stat_recv_pkt_count[track_id]
				, ov::Converter::ToSiString(_stat_recv_pkt_size[track_id], 1).CStr()
			);
		}

		ov::String stat_stream_str = "";

		stat_stream_str.AppendFormat("\n - MediaRouter Stream | type: %s, name: %s/%s, uptime: %lldms, queue: %d, A-V(%lld)" 
			, _inout_type==MRStreamInoutType::Incoming?"Incoming":"Outgoing"
			,_stream->GetApplicationInfo().GetName().CStr()
			,_stream->GetName().CStr()
			,(int64_t)uptime
			, _packets_queue.Size()
			, max_pts - min_pts);

		stat_track_str = stat_stream_str + stat_track_str;

		logtd("%s", stat_track_str.CStr());
	}
}

void MediaRouteStream::DropNonDecodingPackets()
{
	std::map<MediaTrackId, int64_t> _pts_last;

	////////////////////////////////////////////////////////////////////////////////////
	// 1. Discover to the highest PTS value in the keyframe against packets on all tracks.

	std::vector<std::shared_ptr<MediaPacket>> tmp_packets_queue;
	int64_t base_pts = -1LL;

	while(true)
	{
		if(_packets_queue.IsEmpty())
			break;
		
		auto media_packet_ref = _packets_queue.Dequeue();
		if(media_packet_ref.has_value() == false)
			continue;

		auto &media_packet = media_packet_ref.value();

		if(media_packet->GetFlag() == MediaPacketFlag::Key)
		{
			if(base_pts < media_packet->GetPts())
			{
				base_pts = media_packet->GetPts();
			}
		}

		tmp_packets_queue.push_back(std::move(media_packet));
	}	
	// logtw("Discovered base PTS value(%lld)", base_pts);

	////////////////////////////////////////////////////////////////////////////////////
	// 2. Obtain the PTS values for all tracks close to the reference PTS.
	
	// <TrackId, <diff, Pts>>
	std::map<MediaTrackId, std::pair<int64_t, int64_t>> map_near_pts;

	for(auto it = tmp_packets_queue.begin(); it != tmp_packets_queue.end(); it++){
		auto &media_packet = *it;

		if(media_packet->GetFlag() == MediaPacketFlag::Key)
		{
			MediaTrackId track_id = media_packet->GetTrackId();

			int64_t pts_diff = std::abs(media_packet->GetPts() - base_pts);

			auto it_near_pts = map_near_pts.find(track_id);

			if(it_near_pts == map_near_pts.end())
			{
				map_near_pts[track_id] = std::make_pair(pts_diff, media_packet->GetPts());
			}
			else
			{
				auto pair_value = it_near_pts->second;
				int64_t prev_pts_diff = pair_value.first;

				if(prev_pts_diff > pts_diff)
				{
					map_near_pts[track_id] = std::make_pair(pts_diff, media_packet->GetPts());
				}
			}
		}
	}

	////////////////////////////////////////////////////////////////////////////////////
	// 3. Drop all packets below PTS by all tracks

	uint32_t dropeed_packets = 0;
	for(auto it = tmp_packets_queue.begin(); it != tmp_packets_queue.end(); it++)
	{
		auto &media_packet = *it;

		if(media_packet->GetPts() < map_near_pts[media_packet->GetTrackId()].second)
		{
			dropeed_packets++;
			continue;
		}

		_packets_queue.Enqueue(std::move(media_packet));
	}
	tmp_packets_queue.clear();
	
	if(dropeed_packets > 0)
		logtw("Number of dropped packets : %d", dropeed_packets);
}


bool MediaRouteStream::Push(std::shared_ptr<MediaPacket> media_packet)
{	
	////////////////////////////////////////////////////////////////////////////////////
	// [ Calculating Packet Timestamp, Duration] 
	
	// If there is no pts/dts value, do the same as pts/dts value.
	if(media_packet->GetDts() == -1LL)
		media_packet->SetDts(media_packet->GetPts());

	if(media_packet->GetPts() == -1LL)
		media_packet->SetPts(media_packet->GetDts());		

	// Accumulate Packet duplication
	//	- 1) If the current packet does not have a Duration value then stashed.
	//	- 1) If packets stashed, calculate duration compared to the current packet timestamp.
	//	- 3) and then, the current packet stash.

	auto it = _media_packet_stash.find(media_packet->GetTrackId());
	if(it == _media_packet_stash.end())
	{
		_media_packet_stash[media_packet->GetTrackId()] = std::move(media_packet);

		return false;
	}

	auto pop_media_packet = std::move(it->second);

	int64_t duration = media_packet->GetDts() - pop_media_packet->GetDts();
	pop_media_packet->SetDuration(duration);

	_media_packet_stash[media_packet->GetTrackId()] = std::move(media_packet);


	////////////////////////////////////////////////////////////////////////////////////
	// Bitstream format converting to stand format. and, parsing track informaion
	
	auto media_track = _stream->GetTrack(pop_media_packet->GetTrackId());
	if (media_track == nullptr)
	{
		logte("Not found mediatrack. id(%d)", pop_media_packet->GetTrackId());
		return false;
	}	

	switch(GetInoutType())
	{
		case MRStreamInoutType::Incoming:
		{
			bool need_drop_nondecoding_packets = false;
			bool is_parsed_all_track = IsParseTrackAll();

			if(!PushIncomingStream(media_track, pop_media_packet))
			{
				return false;
			}

			if(is_parsed_all_track == false)
			{
				if(IsParseTrackAll() == true)
				{
					need_drop_nondecoding_packets = true;
				}
			}
			
			_packets_queue.Enqueue(std::move(pop_media_packet));

			// Store packets until all track information is parsed.
			if(IsParseTrackAll()==false)
			{
				return false;	
			}

			// Determine whether non-decoding packets are dropped at stream start
			if(need_drop_nondecoding_packets == true)
			{
				DropNonDecodingPackets();
			}

		} break;

		case MRStreamInoutType::Outgoing:
		{
			if(!PushOutgoungStream(media_track, pop_media_packet))
				return false;		

			_packets_queue.Enqueue(std::move(pop_media_packet));
		} break;
		
		default:
		  break;
	}

	return (_packets_queue.Size()>0)?true:false;
}


std::shared_ptr<MediaPacket> MediaRouteStream::Pop()
{
	////////////////////////////////////////////////////////////////////////////////////
	// Peek MediaPacket from queue

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
	// Timestamp change by timebase

	ConvertToDefaultTimestamp(media_track, media_packet);


	////////////////////////////////////////////////////////////////////////////////////
	// PTS Correction for Abnormal increase

	int64_t ts_inc = media_packet->GetPts()  - _pts_last[track_id];
	int64_t ts_inc_ms = ts_inc * 1000 /  media_track->GetTimeBase().GetDen();

	if ( std::abs(ts_inc_ms) > PTS_CORRECT_THRESHOLD_US )
	{
		// TODO(soulk): I think all tracks should calibrate the PTS with the same value.
		_pts_correct[track_id] = media_packet->GetPts() - _pts_last[track_id] - _pts_avg_inc[track_id];

		logtw("Detected abnormal increased pts. track_id : %d, prv_pts : %lld, cur_pts : %lld, crt_pts : %lld, avg_inc : %lld"
			, track_id
			, _pts_last[track_id]
			, media_packet->GetPts()
			, _pts_correct[track_id]
			, _pts_avg_inc[track_id]
		);
	}
	else
	{
		// Originally it should be an average value, Use the difference of the last packet.
		// Use DTS because the PTS value does not increase uniformly.
		_pts_avg_inc[track_id] = media_packet->GetDts() - _dts_last[track_id];
	}

	_pts_last[track_id] = media_packet->GetPts();
	_dts_last[track_id] = media_packet->GetDts();

	media_packet->SetPts( media_packet->GetPts() - _pts_correct[track_id] );
	media_packet->SetDts( media_packet->GetDts() - _pts_correct[track_id] );

	
	////////////////////////////////////////////////////////////////////////////////////
	// Statistics 

	UpdateStatistics(media_track, media_packet);

	return std::move(media_packet);
}


void MediaRouteStream::DumpPacket(
	std::shared_ptr<MediaPacket> &media_packet,
	bool dump)
{
	logtd("track_id(%d), type(%d), fmt(%d), flags(%d), pts(%lld) dts(%lld) dur(%d), size(%d)"
		, media_packet->GetTrackId()
		, media_packet->GetPacketType()
		, media_packet->GetBitstreamFormat()
		, media_packet->GetFlag()
		, media_packet->GetPts() 
		, media_packet->GetDts()
		, media_packet->GetDuration()
		, media_packet->GetDataLength());		

	if(dump == true)
	{
		logtd("%s", media_packet->GetData()->Dump(128).CStr());	
	}
}