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

#include <modules/bitstream/h264/h264_decoder_configuration_record.h>
#include <modules/bitstream/h264/h264_sps.h>
#include <modules/bitstream/h264/h264_avcc_to_annexb.h>
#include <modules/bitstream/h264/h264_nal_unit_types.h>

#include <modules/bitstream/h265/h265_parser.h>
#include <modules/bitstream/h265/h265_types.h>

#include <modules/bitstream/aac/aac_specific_config.h>
#include <modules/bitstream/aac/aac_adts.h>
#include <modules/bitstream/aac/aac_latm_to_adts.h>

#include <modules/bitstream/nalu/nal_unit_fragment_header.h>

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

// Check whether the information extraction for all tracks has been completed.
// TODO(soulk) : Need to performance tuning
bool MediaRouteStream::IsParseTrackAll()
{
	bool is_parse_completed = true;

	for(const auto &iter : _parse_completed_track_info)
	{
		if(iter.second == false)
			return false;
	}

	return is_parse_completed;
}

// Check if an external stream has been generated for the current stream.
bool MediaRouteStream::IsCreatedSteam()
{
	return _created_stream;
}

void MediaRouteStream::SetCreatedSteam(bool created)
{
	_created_stream = created;
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

// Extract codec information from a media packet
bool MediaRouteStream::ParseTrackInfo(std::shared_ptr<MediaTrack> &media_track, std::shared_ptr<MediaPacket> &media_packet)
{
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
				if(frag_hdr != nullptr)
				{
					for (size_t i = 0; i < frag_hdr->GetCount(); ++i) 
					{
						const uint8_t* buffer = &payload_data[frag_hdr->fragmentation_offset[i]];
						size_t length = frag_hdr->fragmentation_length[i];

						uint16_t nal_unit_type = *(buffer) & 0x1F;

						if(nal_unit_type == H264NalUnitType::Sps)
						{
							H264Sps sps;
							if(H264Sps::Parse(buffer, length, sps))
							{
								media_track->SetWidth(sps.GetWidth());
								media_track->SetHeight(sps.GetHeight());
								media_track->SetTimeBase(1, 90000);

								logtd("[%d] need to convert timebase(%d/%d) -> timebase(%d/%d)"
									, media_track->GetId()
									, _incoming_tiembase[media_track->GetId()].GetNum(), _incoming_tiembase[media_track->GetId()].GetDen()
									, media_track->GetTimeBase().GetNum(), media_track->GetTimeBase().GetDen());

								SetParseTrackInfo(media_track, true);
							}
							logtd("%s", sps.GetInfoString().CStr());					
						}
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
				if(frag_hdr != nullptr)
				{
					for (size_t i = 0; i < frag_hdr->GetCount(); ++i) 
					{
						const uint8_t* buffer = &payload_data[frag_hdr->fragmentation_offset[i]];
						size_t length = frag_hdr->fragmentation_length[i];

						/*
						1.1.4.  NAL Unit Header

						   HEVC maintains the NAL unit concept of H.264 with modifications.
						   HEVC uses a two-byte NAL unit header, as shown in Figure 1.  The
						   payload of a NAL unit refers to the NAL unit excluding the NAL unit
						   header.

						            +---------------+---------------+
						            |0|1|2|3|4|5|6|7|0|1|2|3|4|5|6|7|
						            +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
						            |F|   Type    |  LayerId  | TID |
						            +-------------+-----------------+

						   Figure 1: The Structure of the HEVC NAL Unit Header

						   The semantics of the fields in the NAL unit header are as specified
						   in [HEVC] and described briefly below for convenience.  In addition
						   to the name and size of each field, the corresponding syntax element
						   name in [HEVC] is also provided.

						   F: 1 bit
						      forbidden_zero_bit.  Required to be zero in [HEVC].  Note that the
						      inclusion of this bit in the NAL unit header was to enable
						      transport of HEVC video over MPEG-2 transport systems (avoidance
						      of start code emulations) [MPEG2S].  In the context of this memo,



						Wang, et al.                 Standards Track                   [Page 13]
						 
						RFC 7798               RTP Payload Format for HEVC            March 2016


						      the value 1 may be used to indicate a syntax violation, e.g., for
						      a NAL unit resulted from aggregating a number of fragmented units
						      of a NAL unit but missing the last fragment, as described in
						      Section 4.4.3.

						   Type: 6 bits
						      nal_unit_type.  This field specifies the NAL unit type as defined
						      in Table 7-1 of [HEVC].  If the most significant bit of this field
						      of a NAL unit is equal to 0 (i.e., the value of this field is less
						      than 32), the NAL unit is a VCL NAL unit.  Otherwise, the NAL unit
						      is a non-VCL NAL unit.  For a reference of all currently defined
						      NAL unit types and their semantics, please refer to Section 7.4.2
						      in [HEVC].

						   LayerId: 6 bits
						      nuh_layer_id.  Required to be equal to zero in [HEVC].  It is
						      anticipated that in future scalable or 3D video coding extensions
						      of this specification, this syntax element will be used to
						      identify additional layers that may be present in the CVS, wherein
						      a layer may be, e.g., a spatial scalable layer, a quality scalable
						      layer, a texture view, or a depth view.

						   TID: 3 bits
						      nuh_temporal_id_plus1.  This field specifies the temporal
						      identifier of the NAL unit plus 1.  The value of TemporalId is
						      equal to TID minus 1.  A TID value of 0 is illegal to ensure that
						      there is at least one bit in the NAL unit header equal to 1, so to
						      enable independent considerations of start code emulations in the
						      NAL unit header and in the NAL unit payload data.

						*/
						uint8_t nal_unit_type = ( (*(buffer) & 0x7E) >> 1);
						logtd(" [%d] nal_unit_type : %d", frag_hdr->fragmentation_offset[i], nal_unit_type);

						if(nal_unit_type == (uint8_t)H265NALUnitType::SPS)
						{
							H265SPS sps;
							if(H265Parser::ParseSPS(buffer, length, sps))
							{
								media_track->SetWidth(sps.GetWidth());
								media_track->SetHeight(sps.GetHeight());
								media_track->SetTimeBase(1, 90000);

								logtd("[%d] need to convert timebase(%d/%d) -> timebase(%d/%d)"
									, media_track->GetId()
									, _incoming_tiembase[media_track->GetId()].GetNum(), _incoming_tiembase[media_track->GetId()].GetDen()
									, media_track->GetTimeBase().GetNum(), media_track->GetTimeBase().GetDen());

								SetParseTrackInfo(media_track, true);
							}
							logtd("%s", sps.GetInfoString().CStr());					
						}
					}

					//DumpPacket(media_track, media_packet, true);
				}
			} break;
		
		case MediaCodecId::Aac:
			if(media_packet->GetBitstreamFormat() == common::BitstreamFormat::AAC_ADTS)
			{
				if(AACAdts::IsValid(media_packet->GetData()->GetDataAs<uint8_t>(), media_packet->GetDataLength()) == true)
				{
					AACAdts adts;
					if(AACAdts::Parse(media_packet->GetData()->GetDataAs<uint8_t>(), media_packet->GetDataLength(), adts) == true)
					{					
						media_track->SetSampleRate(adts.SamplerateNum());
						media_track->GetChannel().SetLayout( (adts.SamplerateNum()==1)?(AudioChannel::Layout::LayoutMono):(AudioChannel::Layout::LayoutStereo) );

						media_track->SetTimeBase(1, media_track->GetSampleRate());

						logtd("[%d] need to convert timebase(%d/%d) -> timebase(%d/%d)"
							, media_track->GetId()
							, _incoming_tiembase[media_track->GetId()].GetNum(), _incoming_tiembase[media_track->GetId()].GetDen()
							, media_track->GetTimeBase().GetNum(), media_track->GetTimeBase().GetDen());

						SetParseTrackInfo(media_track, true);
					}
					logtd("%s", adts.GetInfoString().CStr());
				}
			}		
			break;

		// The incoming stream does not support this codec.
		case MediaCodecId::Vp8: 
		case MediaCodecId::Opus:
		case MediaCodecId::Vp9:
		case MediaCodecId::Mp3:
			logte("Not support codec in incoming stream");
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

		} break;

		default:
		break;
	}

	// KeyFrame
	media_packet->SetFlag(MediaPacketFlag::Key);

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
					logte("Bitstream format change failed");
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
					logte("Bitstream format change failed");
					return false;
				}
			
				media_packet->SetBitstreamFormat(common::BitstreamFormat::AAC_ADTS);
				media_packet->SetPacketType(common::PacketType::RAW);
			}
			break;
		
		// The incoming stream does not support this codec.
		case MediaCodecId::H265:
			// TODO(Soulk) : Implement this
			return true;
		case MediaCodecId::Vp8: 
		case MediaCodecId::Vp9:
		case MediaCodecId::Opus:
		case MediaCodecId::Mp3:
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

	if(_incoming_tiembase[media_track->GetId()] == media_track->GetTimeBase())
	{
		// If the timebase is the same, there is no need to change the timestamp.
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

			temp_str.AppendFormat("\n\t[%3d] type: %5s(%2d/%4s), %s, pkt_cnt: %6lld, pkt_siz: %sB"
				, track_id
				, track->GetMediaType()==MediaType::Video?"video":"audio"
				, track->GetCodecId()
				, ov::Converter::ToString(track->GetCodecId()).CStr()
				, pts_str.CStr()
				, _stat_recv_pkt_count[track_id]
				, ov::Converter::ToSiString(_stat_recv_pkt_size[track_id], 1).CStr()
			);
		}

		logtd("%s", temp_str.CStr());
	}
}


bool MediaRouteStream::Push(std::shared_ptr<MediaPacket> media_packet)
{	
	////////////////////////////////////////////////////////////////////////////////////
	// Bitstream format converting to stand format. and, parsing track informaion
	////////////////////////////////////////////////////////////////////////////////////
	auto media_track = _stream->GetTrack(media_packet->GetTrackId());
	if (media_track == nullptr)
	{
		logte("Not found mediatrack. id(%d)", media_packet->GetTrackId());
		return false;
	}	

	if(GetInoutType() == MRStreamInoutType::Incoming)
	{
		if(!PushIncomingStream(media_track, media_packet))
			return false;
	}
	else if(GetInoutType() == MRStreamInoutType::Outgoing)
	{
		if(!PushOutgoungStream(media_track, media_packet))
			return false;			
	}

	////////////////////////////////////////////////////////////////////////////////////
	// Accumulate Packet duplication
	//	- 1) If packets stored in temporary storage exist, calculate Duration compared to the current packet's timestamp.
	//	- 2) If the current packet does not have a Duration value, keep it in a temporary store.
	//	- 3) If there is a packet Duration value, insert it into the packet queue.
	////////////////////////////////////////////////////////////////////////////////////
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

	if(media_packet->GetDuration() <= 0)
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
	////////////////////////////////////////////////////////////////////////////////////
	// Peek MediaPacket from queue
	////////////////////////////////////////////////////////////////////////////////////
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
	////////////////////////////////////////////////////////////////////////////////////
	ConvertToDefaultTimestamp(media_track, media_packet);


	////////////////////////////////////////////////////////////////////////////////////
	// PTS Correction for Abnormal increase
	////////////////////////////////////////////////////////////////////////////////////
	int64_t timestamp_delta = media_packet->GetPts()  - _stat_recv_pkt_lpts[track_id];
	int64_t scaled_timestamp_delta = timestamp_delta * 1000 /  media_track->GetTimeBase().GetDen();

	if (abs( scaled_timestamp_delta ) > PTS_CORRECT_THRESHOLD_US )
	{
		// TODO(soulk): I think all tracks should calibrate the PTS with the same value.
		_pts_correct[track_id] = media_packet->GetPts() - _stat_recv_pkt_lpts[track_id] - _pts_avg_inc[track_id];
/*
		logtw("Detected abnormal increased pts. track_id : %d, prv_pts : %lld, cur_pts : %lld, crt_pts : %lld, avg_inc : %lld"
			, track_id
			, _stat_recv_pkt_lpts[track_id]
			, media_packet->GetPts()
			, _pts_correct[track_id]
			, _pts_avg_inc[track_id]
		);
*/		
	}
	else
	{
		// Originally it should be an average value, Use the difference of the last packet.
		// Use DTS because the PTS value does not increase uniformly.
		_pts_avg_inc[track_id] = media_packet->GetDts() - _stat_recv_pkt_ldts[track_id];
	}

	// Set the corrected PTS.
	media_packet->SetPts( media_packet->GetPts() - _pts_correct[track_id] );
	media_packet->SetDts( media_packet->GetDts() - _pts_correct[track_id] );


	////////////////////////////////////////////////////////////////////////////////////
	// Statistics for log
	////////////////////////////////////////////////////////////////////////////////////
	UpdateStatistics(media_track, media_packet);


	return media_packet;
}


void MediaRouteStream::DumpPacket(
	std::shared_ptr<MediaTrack> &media_track,
	std::shared_ptr<MediaPacket> &media_packet,
	bool dump)
{
	logtd("track_id(%d), type(%d), fmt(%d), flags(%d), pts(%lld) dts(%lld)"
		, media_packet->GetTrackId()
		, media_packet->GetPacketType()
		, media_packet->GetBitstreamFormat()
		, media_packet->GetFlag()
		, media_packet->GetPts() 
		, media_packet->GetDts());		

	if(dump == true)
	{
		logtd("%s", media_packet->GetData()->Dump(128).CStr());	
	}
}