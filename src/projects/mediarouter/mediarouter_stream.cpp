//==============================================================================
//
//  MediaRouteStream
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

// Role Definition
// ------------------------
// Inbound stream process
//		Calculating packet duration ->
//		Changing the bitstream format  ->
// 		Parsing Track Information ->
// 		Parsing Fragmentation Header ->
// 		Parsing Key-frame ->
// 		Append Decoder PrameterSets ->
// 		Changing the timestamp based on the timebase

// Outbound Stream process
//		Calculating packet duration ->
// 		Parsing Track Information ->
// 		Parsing Fragmentation Header ->
// 		Parsing Key-frame ->
// 		Append Decoder PrameterSets ->
// 		Changing the timestamp based on the timebase

#include "mediarouter_stream.h"

#include <base/ovlibrary/ovlibrary.h>
#include <modules/bitstream/aac/aac_adts.h>
#include <modules/bitstream/aac/aac_converter.h>
#include <modules/bitstream/aac/aac_specific_config.h>
#include <modules/bitstream/h264/h264_converter.h>
#include <modules/bitstream/h264/h264_decoder_configuration_record.h>
#include <modules/bitstream/h264/h264_nal_unit_types.h>
#include <modules/bitstream/h264/h264_parser.h>
#include <modules/bitstream/h265/h265_parser.h>
#include <modules/bitstream/nalu/nal_unit_fragment_header.h>
#include <modules/bitstream/opus/opus.h>
#include <modules/bitstream/vp8/vp8.h>

#include "mediarouter_private.h"

#define PTS_CORRECT_THRESHOLD_MS 3000

using namespace cmn;

MediaRouteStream::MediaRouteStream(const std::shared_ptr<info::Stream> &stream)
	: _stream(stream),
	  _packets_queue(nullptr, 100)
{
	logti("Trying to create media route stream: name(%s) id(%u)", stream->GetName().CStr(), stream->GetId());
	_inout_type = MediaRouterStreamType::UNKNOWN;

	_max_warning_count_bframe = 0;

	_stat_start_time = std::chrono::system_clock::now();
	_stop_watch.Start();
}

MediaRouteStream::MediaRouteStream(const std::shared_ptr<info::Stream> &stream, MediaRouterStreamType inout_type)
	: MediaRouteStream(stream)
{
	SetInoutType(inout_type);
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
}

std::shared_ptr<info::Stream> MediaRouteStream::GetStream()
{
	return _stream;
}

void MediaRouteStream::SetInoutType(MediaRouterStreamType inout_type)
{
	_inout_type = inout_type;

	_packets_queue.SetAlias(ov::String::FormatString("%s/%s - Mediarouter stream packet %s", _stream->GetApplicationInfo().GetName().CStr(), _stream->GetName().CStr(), (_inout_type == MediaRouterStreamType::INBOUND) ? "Inbound" : "Outbound"));
}

MediaRouterStreamType MediaRouteStream::GetInoutType()
{
	return _inout_type;
}

void MediaRouteStream::OnStreamPrepared(bool completed)
{
	_is_stream_prepared = completed;
}

bool MediaRouteStream::IsStreamPrepared()
{
	return _is_stream_prepared;
}

void MediaRouteStream::Flush()
{
	// Clear queued apckets
	_packets_queue.Clear();
	// Clear stahsed Packets
	_media_packet_stash.clear();

	_are_all_tracks_parsed = false;

	_is_stream_prepared = false;
}

#include <base/ovcrypto/base_64.h>
bool MediaRouteStream::ProcessH264AVCCStream(std::shared_ptr<MediaTrack> &media_track, std::shared_ptr<MediaPacket> &media_packet)
{
	// Everytime : Convert to AnnexB, Make fragment header, Set keyframe flag, Appned SPS/PPS nal units in front of IDR frame
	// one time : Parse track info from sps/pps and generate codec_extra_data

	if (media_packet->GetBitstreamFormat() != cmn::BitstreamFormat::H264_AVCC)
	{
		return false;
	}

	if (media_packet->GetPacketType() == cmn::PacketType::SEQUENCE_HEADER)
	{
		// Validation
		auto data = media_packet->GetData()->GetDataAs<uint8_t>();
		auto length = media_packet->GetData()->GetLength();
		AVCDecoderConfigurationRecord config;
		if (!AVCDecoderConfigurationRecord::Parse(data, length, config))
		{
			logte("Could not parse sequence header");
			return false;
		}

		if (config.NumOfSPS() <= 0 || config.NumOfPPS() <= 0)
		{
			logte("There is no SPS/PPS in the sequence header");
			return false;
		}

		logtd("[SPS] %s [PPS] %s", ov::Base64::Encode(config.GetSPS(0)).CStr(), ov::Base64::Encode(config.GetPPS(0)).CStr());

		if (config.NumOfSPS() > 0)
		{
			auto sps_data = config.GetSPS(0);
			H264SPS sps;
			if (H264Parser::ParseSPS(sps_data->GetDataAs<uint8_t>(), sps_data->GetLength(), sps) == false)
			{
				logte("Could not parse H264 SPS unit");
				return false;
			}
			
			media_track->SetH264SpsData(sps_data);
			media_track->SetWidth(sps.GetWidth());
			media_track->SetHeight(sps.GetHeight());
		}
		
		if (config.NumOfPPS() > 0)
		{
			auto pps_data = config.GetPPS(0);
			media_track->SetH264PpsData(pps_data);
		}

		media_track->SetCodecExtradata(media_packet->GetData());
		auto [sps_pps_data, frag_header] = config.GetSpsPpsAsAnnexB(4);
		media_track->SetH264SpsPpsAnnexBFormat(sps_pps_data, frag_header);

		return false;
	}
	// Convert to AnnexB and Insert SPS/PPS if there are no SPS/PPS nal units.
	else if (media_packet->GetPacketType() == cmn::PacketType::NALU)
	{
		auto converted_data = std::make_shared<ov::Data>(media_packet->GetDataLength() + (media_packet->GetDataLength() / 2));
		FragmentationHeader fragment_header;
		size_t nalu_offset = 0;

		const uint8_t START_CODE[4] = {0x00, 0x00, 0x00, 0x01};
		const size_t START_CODE_LEN = sizeof(START_CODE);

		bool has_idr = false;
		bool has_sps = false;
		bool has_pps = false;

		ov::ByteStream read_stream(media_packet->GetData());
		while (read_stream.Remained() > 0)
		{
			if (read_stream.IsRemained(4) == false)
			{
				logte("Not enough data to parse NAL");
				return false;
			}

			size_t nal_length = read_stream.ReadBE32();
			if (read_stream.IsRemained(nal_length) == false)
			{
				logte("NAL length (%d) is greater than buffer length (%d)", nal_length, read_stream.Remained());
				return false;
			}

			// Convert to AnnexB
			auto nalu = read_stream.GetRemainData(nal_length);

			// Exception handling for encoder that transmits AVCC in non-standard format ([Size][Start Code][NalU])
			if ((nalu->GetLength() > 3 && nalu->GetDataAs<uint8_t>()[0] == 0x00 && nalu->GetDataAs<uint8_t>()[1] == 0x00 && nalu->GetDataAs<uint8_t>()[2] == 0x01) ||
				(nalu->GetLength() > 4 && nalu->GetDataAs<uint8_t>()[0] == 0x00 && nalu->GetDataAs<uint8_t>()[1] == 0x00 && nalu->GetDataAs<uint8_t>()[2] == 0x00 && nalu->GetDataAs<uint8_t>()[3] == 0x01))
			{
				size_t start_code_size = (nalu->GetDataAs<uint8_t>()[2] == 0x01) ? 3 : 4;

				read_stream.Skip(start_code_size);
				nal_length -= start_code_size;
				nalu = read_stream.GetRemainData(nal_length);
			}

			[[maybe_unused]] auto skipped = read_stream.Skip(nal_length);
			OV_ASSERT2(skipped == nal_length);

			H264NalUnitHeader nal_header;
			if (H264Parser::ParseNalUnitHeader(nalu->GetDataAs<uint8_t>(), H264_NAL_UNIT_HEADER_SIZE, nal_header) == true)
			{
				// logtd("nal_unit_type : %s", NalUnitTypeToStr((uint8_t)nal_header.GetNalUnitType()).CStr());

				if (nal_header.GetNalUnitType() == H264NalUnitType::IdrSlice)
				{
					media_packet->SetFlag(MediaPacketFlag::Key);
					has_idr = true;
				}
				else if (nal_header.GetNalUnitType() == H264NalUnitType::Sps)
				{
					// logtd("[SPS] %s ", ov::Base64::Encode(nalu).CStr());
					has_sps = true;
				}
				else if (nal_header.GetNalUnitType() == H264NalUnitType::Pps)
				{
					// logtd("[PPS] %s ", ov::Base64::Encode(nalu).CStr());
					has_pps = true;
				}
			}

			converted_data->Append(START_CODE, sizeof(START_CODE));
			nalu_offset += START_CODE_LEN;

			fragment_header.fragmentation_offset.push_back(nalu_offset);
			fragment_header.fragmentation_length.push_back(nalu->GetLength());

			converted_data->Append(nalu);
			nalu_offset += nalu->GetLength();
		}

		if (has_idr == true && (has_sps == false || has_pps == false) && media_track->GetH264SpsPpsAnnexBFormat() != nullptr)
		{
			// Insert SPS/PPS nal units so that player can start to play faster
			auto processed_data = std::make_shared<ov::Data>(65535);
			auto decode_parmeters = media_track->GetH264SpsPpsAnnexBFormat();

			processed_data->Append(decode_parmeters);
			processed_data->Append(converted_data);

			// Update fragment haeader
			FragmentationHeader updated_frag_header;
			updated_frag_header.fragmentation_offset = media_track->GetH264SpsPpsAnnexBFragmentHeader().fragmentation_offset;
			updated_frag_header.fragmentation_length = media_track->GetH264SpsPpsAnnexBFragmentHeader().fragmentation_length;

			// Existring fragment header offset because SPS/PPS was inserted at front
			auto offset_offset = updated_frag_header.fragmentation_offset.back() + updated_frag_header.fragmentation_length.back();
			for (size_t i = 0; i < fragment_header.fragmentation_offset.size(); i++)
			{
				size_t updated_offset = fragment_header.fragmentation_offset[i] + offset_offset;
				updated_frag_header.fragmentation_offset.push_back(updated_offset);
				updated_frag_header.fragmentation_length.push_back(fragment_header.fragmentation_length[i]);
			}

			media_packet->SetFragHeader(&updated_frag_header);
			media_packet->SetData(processed_data);
		}
		else
		{
			media_packet->SetFragHeader(&fragment_header);
			media_packet->SetData(converted_data);
		}

		media_packet->SetBitstreamFormat(cmn::BitstreamFormat::H264_ANNEXB);
		media_packet->SetPacketType(cmn::PacketType::NALU);

		return true;
	}

	return false;
}

bool MediaRouteStream::ProcessH264AnnexBStream(std::shared_ptr<MediaTrack> &media_track, std::shared_ptr<MediaPacket> &media_packet)
{
	// Everytime : Make fragment header, Set keyframe flag, Appned SPS/PPS nal units in front of IDR frame
	// one time : Parse track info from sps/pps and generate codec_extra_data

	AVCDecoderConfigurationRecord avc_decoder_configuration_record;
	FragmentationHeader fragment_header;
	size_t offset = 0, offset_length = 0;
	auto bitstream = media_packet->GetData()->GetDataAs<uint8_t>();
	auto bitstream_length = media_packet->GetData()->GetLength();
	bool has_sps = false, has_pps = false, has_idr = false;
	size_t annexb_start_code_size = 0;

	while (offset < bitstream_length)
	{
		size_t start_code_size;
		int pos = 0;

		// Find Offset
		pos = H264Parser::FindAnnexBStartCode(bitstream + offset, bitstream_length - offset, start_code_size);
		if (pos == -1)
		{
			break;
		}

		if (annexb_start_code_size == 0)
		{
			annexb_start_code_size = start_code_size;
		}

		offset += pos + start_code_size;

		// Find length and next offset
		pos = H264Parser::FindAnnexBStartCode(bitstream + offset, bitstream_length - offset, start_code_size);
		if (pos == -1)
		{
			// Last NALU
			offset_length = bitstream_length - offset;
		}
		else
		{
			offset_length = pos;
		}

		fragment_header.fragmentation_offset.push_back(offset);
		fragment_header.fragmentation_length.push_back(offset_length);

		H264NalUnitHeader nal_header;
		if (H264Parser::ParseNalUnitHeader(bitstream + offset, offset_length, nal_header) == false)
		{
			logte("Could not parse H264 Nal unit header");
			return false;
		}
		// logtd("nal_unit_type : %s", NalUnitTypeToStr((uint8_t)nal_header.GetNalUnitType()).CStr());

		if (nal_header.GetNalUnitType() == H264NalUnitType::Sps)
		{
			has_sps = true;

			// logtd("[-SPS] %s ", ov::Base64::Encode(nalu).CStr());

			// Parse track info if needed
			if (media_track->IsValid() == false)
			{
				auto nalu = std::make_shared<ov::Data>(bitstream + offset, offset_length);
				H264SPS sps;
				if (H264Parser::ParseSPS(nalu->GetDataAs<uint8_t>(), nalu->GetLength(), sps) == false)
				{
					logte("Could not parse H264 SPS unit");
					return false;
				}

				media_track->SetH264SpsData(nalu);
				media_track->SetWidth(sps.GetWidth());
				media_track->SetHeight(sps.GetHeight());

				logtd("%s", sps.GetInfoString().CStr());

				avc_decoder_configuration_record.AddSPS(nalu);
				avc_decoder_configuration_record.SetProfileIndication(sps.GetProfileIdc());
				avc_decoder_configuration_record.SetCompatibility(sps.GetConstraintFlag());
				avc_decoder_configuration_record.SetlevelIndication(sps.GetCodecLevelIdc());
			}
		}
		else if (nal_header.GetNalUnitType() == H264NalUnitType::Pps)
		{
			has_pps = true;

			// logtd("[-PPS] %s ", ov::Base64::Encode(nalu).CStr());

			// Parse track info if needed
			if (media_track->IsValid() == false)
			{
				auto nalu = std::make_shared<ov::Data>(bitstream + offset, offset_length);
				media_track->SetH264PpsData(nalu);
				avc_decoder_configuration_record.AddPPS(nalu);
			}
		}
		else if (nal_header.GetNalUnitType() == H264NalUnitType::IdrSlice)
		{
			has_idr = true;
			media_packet->SetFlag(MediaPacketFlag::Key);
		}

		// Last NalU
		if (pos == -1)
		{
			break;
		}

		offset += pos;
	}

	if (media_track->IsValid() == false &&
		avc_decoder_configuration_record.NumOfSPS() > 0 && avc_decoder_configuration_record.NumOfPPS() > 0)
	{
		avc_decoder_configuration_record.SetVersion(1);
		// Set default nal unit length
		avc_decoder_configuration_record.SetLengthOfNalUnit(3);
		media_track->SetCodecExtradata(avc_decoder_configuration_record.Serialize());
		auto [sps_pps_annexb_data, sps_pps_frag_header] = avc_decoder_configuration_record.GetSpsPpsAsAnnexB(annexb_start_code_size);
		media_track->SetH264SpsPpsAnnexBFormat(sps_pps_annexb_data, sps_pps_frag_header);
	}

	if (has_idr == true && (has_sps == false || has_pps == false) && media_track->GetH264SpsPpsAnnexBFormat() != nullptr)
	{
		// Insert SPS/PPS nal units so that player can start to play faster
		auto processed_data = std::make_shared<ov::Data>(media_packet->GetData()->GetLength() + 1024);
		auto decode_parmeters = media_track->GetH264SpsPpsAnnexBFormat();
		processed_data->Append(decode_parmeters);
		processed_data->Append(media_packet->GetData());

		// Update fragment haeader
		FragmentationHeader updated_frag_header;
		updated_frag_header.fragmentation_offset = media_track->GetH264SpsPpsAnnexBFragmentHeader().fragmentation_offset;
		updated_frag_header.fragmentation_length = media_track->GetH264SpsPpsAnnexBFragmentHeader().fragmentation_length;

		// Existring fragment header offset because SPS/PPS was inserted at front
		auto offset_offset = updated_frag_header.fragmentation_offset.back() + updated_frag_header.fragmentation_length.back();
		for (size_t i = 0; i < fragment_header.fragmentation_offset.size(); i++)
		{
			size_t updated_offset = fragment_header.fragmentation_offset[i] + offset_offset;
			updated_frag_header.fragmentation_offset.push_back(updated_offset);
			updated_frag_header.fragmentation_length.push_back(fragment_header.fragmentation_length[i]);
		}

		media_packet->SetFragHeader(&updated_frag_header);
		media_packet->SetData(processed_data);
	}
	else
	{
		media_packet->SetFragHeader(&fragment_header);
	}

	return true;
}

bool MediaRouteStream::ProcessAACRawStream(std::shared_ptr<MediaTrack> &media_track, std::shared_ptr<MediaPacket> &media_packet)
{
	media_packet->SetFlag(MediaPacketFlag::Key);
	// everytime : Convert to ADTS
	// one time : Parse sequence header
	if (media_packet->GetPacketType() == cmn::PacketType::SEQUENCE_HEADER)
	{
		// Validation
		auto data = media_packet->GetData()->GetDataAs<uint8_t>();
		auto length = media_packet->GetData()->GetLength();

		AACSpecificConfig config;
		if (!AACSpecificConfig::Parse(data, length, config))
		{
			logte("aac sequence header paring error");
			return false;
		}

		media_track->SetSampleRate(config.SamplerateNum());
		media_track->GetChannel().SetLayout(config.Channel() == 1 ? AudioChannel::Layout::LayoutMono : AudioChannel::Layout::LayoutStereo);
		media_track->SetCodecExtradata(media_packet->GetData());
		media_track->SetAacConfig(std::make_shared<AACSpecificConfig>(config));

		return false;
	}
	else
	{
		if (media_track->IsValid() == false)
		{
			// Track information has not been parsed yet.
			logte("Raw aac sequence header has not parsed yet.");
			return false;
		}

		// Convert to adts (raw aac data should be 1 frame)
		auto adts_data = AacConverter::ConvertRawToAdts(media_packet->GetData(), media_track->GetAacConfig());
		if (adts_data == nullptr)
		{
			logte("Failed to convert raw aac to adts.");
			return false;
		}

		media_packet->SetData(adts_data);
		media_packet->SetBitstreamFormat(cmn::BitstreamFormat::AAC_ADTS);
		media_packet->SetPacketType(cmn::PacketType::RAW);
	}

	return true;
}

bool MediaRouteStream::ProcessAACAdtsStream(std::shared_ptr<MediaTrack> &media_track, std::shared_ptr<MediaPacket> &media_packet)
{
	// One time : Parse track information

	media_packet->SetFlag(MediaPacketFlag::Key);

	// AAC ADTS only needs to analyze the track information of the media stream once.
	if (media_track->IsValid() == true)
	{
		return true;
	}

	AACAdts adts;
	if (AACAdts::Parse(media_packet->GetData()->GetDataAs<uint8_t>(), media_packet->GetDataLength(), adts) == false)
	{
		logte("Could not parse AAC ADTS header");
		return false;
	}

	media_track->SetSampleRate(adts.SamplerateNum());
	media_track->GetChannel().SetLayout((adts.ChannelConfiguration() == 1) ? (AudioChannel::Layout::LayoutMono) : (AudioChannel::Layout::LayoutStereo));

	// for extradata
	auto aac_config = std::make_shared<AACSpecificConfig>();

	aac_config->SetOjbectType(adts.Profile());
	aac_config->SetSamplingFrequency(adts.Samplerate());
	aac_config->SetChannel(adts.ChannelConfiguration());

	media_track->SetCodecExtradata(aac_config->Serialize());
	media_track->SetAacConfig(aac_config);

	return true;
}

bool MediaRouteStream::ProcessH265AnnexBStream(std::shared_ptr<MediaTrack> &media_track, std::shared_ptr<MediaPacket> &media_packet)
{
	// Everytime : Generate fragmentation header, Check key frame
	// One time : Parse SPS and Set width/height (track information)

	// TODO : Appned SPS/PPS nal units in front of IDR frame

	auto bitstream = media_packet->GetData()->GetDataAs<uint8_t>();
	auto bitstream_length = media_packet->GetData()->GetLength();
	FragmentationHeader fragment_header;

	size_t offset = 0, offset_length = 0;
	while (offset < bitstream_length)
	{
		size_t start_code_size;
		int pos = 0;

		// Find Offset
		pos = H265Parser::FindAnnexBStartCode(bitstream + offset, bitstream_length - offset, start_code_size);
		if (pos == -1)
		{
			break;
		}

		offset += pos + start_code_size;

		// Find length and next offset
		pos = H265Parser::FindAnnexBStartCode(bitstream + offset, bitstream_length - offset, start_code_size);
		if (pos == -1)
		{
			// Last NALU
			offset_length = bitstream_length - offset;
		}
		else
		{
			offset_length = pos;
		}

		fragment_header.fragmentation_offset.push_back(offset);
		fragment_header.fragmentation_length.push_back(offset_length);

		H265NalUnitHeader header;
		if (H265Parser::ParseNalUnitHeader(bitstream + offset, H265_NAL_UNIT_HEADER_SIZE, header) == false)
		{
			logte("Could not parse H265 Nal unit header");
			return false;
		}

		// Key Frame
		if (header.GetNalUnitType() == H265NALUnitType::IDR_W_RADL ||
			header.GetNalUnitType() == H265NALUnitType::CRA_NUT ||
			header.GetNalUnitType() == H265NALUnitType::BLA_W_RADL)
		{
			media_packet->SetFlag(MediaPacketFlag::Key);
			return true;
		}

		// Track info
		if (media_track->IsValid() == false)
		{
			if (header.GetNalUnitType() == H265NALUnitType::SPS)
			{
				H265SPS sps;
				if (H265Parser::ParseSPS(bitstream + offset, offset_length, sps) == false)
				{
					logte("Could not parse H265 SPS Unit");

					return false;
				}

				media_track->SetWidth(sps.GetWidth());
				media_track->SetHeight(sps.GetHeight());
			}
		}
	}

	return true;
}

bool MediaRouteStream::ProcessVP8Stream(std::shared_ptr<MediaTrack> &media_track, std::shared_ptr<MediaPacket> &media_packet)
{
	// One time : parse width, height
	if (media_track->IsValid() == true)
	{
		return true;
	}

	VP8Parser parser;
	if (VP8Parser::Parse(media_packet->GetData()->GetDataAs<uint8_t>(), media_packet->GetDataLength(), parser) == false)
	{
		logte("Could not parse VP8 header");
		return false;
	}

	media_track->SetWidth(parser.GetWidth());
	media_track->SetHeight(parser.GetHeight());

	if (parser.IsKeyFrame())
	{
		media_packet->SetFlag(MediaPacketFlag::Key);
	}

	return true;
}

bool MediaRouteStream::ProcessOPUSStream(std::shared_ptr<MediaTrack> &media_track, std::shared_ptr<MediaPacket> &media_packet)
{
	// One time : parse samplerate, channel
	if (media_track->IsValid() == true)
	{
		return true;
	}

	OPUSParser parser;
	if (OPUSParser::Parse(media_packet->GetData()->GetDataAs<uint8_t>(), media_packet->GetDataLength(), parser) == false)
	{
		logte("Could not parse OPUS header");
		return false;
	}

	// The opus has a fixed samplerate of 48000
	media_track->SetSampleRate(48000);
	media_track->GetChannel().SetLayout((parser.GetStereoFlag() == 0) ? (AudioChannel::Layout::LayoutMono) : (AudioChannel::Layout::LayoutStereo));

	return true;
}

bool MediaRouteStream::ProcessInboundStream(std::shared_ptr<MediaTrack> &media_track, std::shared_ptr<MediaPacket> &media_packet)
{
	bool result = false;

	switch (media_packet->GetBitstreamFormat())
	{
		case cmn::BitstreamFormat::H264_ANNEXB:
			result = ProcessH264AnnexBStream(media_track, media_packet);
			break;
		case cmn::BitstreamFormat::H264_AVCC:
			result = ProcessH264AVCCStream(media_track, media_packet);
			break;
		case cmn::BitstreamFormat::H265_ANNEXB:
			result = ProcessH265AnnexBStream(media_track, media_packet);
			break;
		case cmn::BitstreamFormat::VP8:
			result = ProcessVP8Stream(media_track, media_packet);
			break;
		case cmn::BitstreamFormat::AAC_RAW:
			result = ProcessAACRawStream(media_track, media_packet);
			break;
		case cmn::BitstreamFormat::AAC_ADTS:
			result = ProcessAACAdtsStream(media_track, media_packet);
			break;
		case cmn::BitstreamFormat::OPUS:
			result = ProcessOPUSStream(media_track, media_packet);
			break;
		case cmn::BitstreamFormat::AAC_LATM:
		case cmn::BitstreamFormat::JPEG:
		case cmn::BitstreamFormat::PNG:
		case cmn::BitstreamFormat::Unknown:
		default:
			logte("Bitstream not supported by inbound");
			break;
	}

	return result;
}

bool MediaRouteStream::ProcessOutboundStream(std::shared_ptr<MediaTrack> &media_track, std::shared_ptr<MediaPacket> &media_packet)
{
	bool result = false;

	switch (media_packet->GetBitstreamFormat())
	{
		case cmn::BitstreamFormat::H264_ANNEXB:
			result = ProcessH264AnnexBStream(media_track, media_packet);
			break;
		case cmn::BitstreamFormat::H264_AVCC:
			result = ProcessH264AVCCStream(media_track, media_packet);
			break;
		case cmn::BitstreamFormat::H265_ANNEXB:
			result = ProcessH265AnnexBStream(media_track, media_packet);
			break;
		case cmn::BitstreamFormat::VP8:
			result = ProcessVP8Stream(media_track, media_packet);
			break;
		case cmn::BitstreamFormat::AAC_RAW:
			result = ProcessAACRawStream(media_track, media_packet);
			break;
		case cmn::BitstreamFormat::AAC_ADTS:
			result = ProcessAACAdtsStream(media_track, media_packet);
			break;
		case cmn::BitstreamFormat::OPUS:
			result = ProcessOPUSStream(media_track, media_packet);
			break;
		case cmn::BitstreamFormat::JPEG:
		case cmn::BitstreamFormat::PNG:
			result = true;
			break;
		case cmn::BitstreamFormat::AAC_LATM:
		case cmn::BitstreamFormat::Unknown:
		default:
			logte("Bitstream not supported by outbound");
			break;
	}

	return result;
}

// Check whether the information extraction for all tracks has been completed.
bool MediaRouteStream::AreAllTracksParsed()
{
	if (_are_all_tracks_parsed == true)
	{
		return true;
	}

	auto tracks = _stream->GetTracks();

	for (const auto &track_it : tracks)
	{
		auto track = track_it.second;
		if (track->IsValid() == false)
		{
			return false;
		}
	}

	_are_all_tracks_parsed = true;

	return true;
}

void MediaRouteStream::UpdateStatistics(std::shared_ptr<MediaTrack> &media_track, std::shared_ptr<MediaPacket> &media_packet)
{
	auto track_id = media_track->GetId();

	// Check b-frame of H264/H265 codec
	//
	// Basically, in order to check the presence of B-Frame, the SliceType of H264 should be checked,
	// but in general, it is assumed that there is a B-frame when PTS and DTS are different. This has a performance advantage.
	switch (media_packet->GetBitstreamFormat())
	{
		case cmn::BitstreamFormat::H264_ANNEXB:
		case cmn::BitstreamFormat::H264_AVCC:
		case cmn::BitstreamFormat::H265_ANNEXB:
			if (_max_warning_count_bframe < 10)
			{
				if (_stat_recv_pkt_count[track_id] > 0 && _stat_recv_pkt_lpts[track_id] > media_packet->GetPts())
				{
					media_track->SetBframes(true);
				}

				// Display a warning message that b-frame exists
				if (media_track->HasBframes() == true)
				{
					logtw("b-frame has been detected in the %d track of %s %s/%s stream",
						  track_id,
						  _inout_type == MediaRouterStreamType::INBOUND ? "inbound" : "outbound",
						  _stream->GetApplicationInfo().GetName().CStr(),
						  _stream->GetName().CStr());

					_max_warning_count_bframe++;
				}
			}
			break;
		default:
			break;
	}

	_stat_recv_pkt_lpts[track_id] = media_packet->GetPts();
	_stat_recv_pkt_ldts[track_id] = media_packet->GetDts();
	_stat_recv_pkt_size[track_id] += media_packet->GetData()->GetLength();
	_stat_recv_pkt_count[track_id]++;


	// 	Diffrence time of received first packet with uptime.
	if (_stat_first_time_diff[track_id] == 0)
	{
		int64_t uptime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - _stat_start_time).count();

		int64_t rescaled_last_pts = (int64_t)((double)(_stat_recv_pkt_lpts[track_id] * 1000) * _stream->GetTrack(track_id)->GetTimeBase().GetExpr());

		_stat_first_time_diff[track_id] = uptime - rescaled_last_pts;
	}

	if (_stop_watch.IsElapsed(10000) && _stop_watch.Update())
	{
		// Uptime
		int64_t uptime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - _stat_start_time).count();

		int64_t min_pts = -1LL;
		int64_t max_pts = -1LL;

		ov::String stat_track_str = "";

		for (const auto &iter : _stream->GetTracks())
		{
			auto track_id = iter.first;
			auto track = iter.second;

			int64_t rescaled_last_pts = (int64_t)((double)(_stat_recv_pkt_lpts[track_id] * 1000) * track->GetTimeBase().GetExpr());
			int64_t first_delay = _stat_first_time_diff[track_id];
			int64_t last_delay = uptime - rescaled_last_pts;

			// calc min/max pts
			if (min_pts == -1LL)
			{
				min_pts = rescaled_last_pts;
			}

			if (max_pts == -1LL)
			{
				max_pts = rescaled_last_pts;
			}

			min_pts = std::min(min_pts, rescaled_last_pts);
			max_pts = std::max(max_pts, rescaled_last_pts);

			stat_track_str.AppendFormat("\n\ttrack:%3d, type: %4s, codec: %4s(%2d), pts: %lldms(%5lldms), tb: %d/%5d, pkt_cnt: %6lld, pkt_siz: %sB, bps: %dKbps",
										track_id,
										track->GetMediaType() == MediaType::Video ? "video" : "audio",
										::StringFromMediaCodecId(track->GetCodecId()).CStr(),
										track->GetCodecId(),
										rescaled_last_pts,
										(first_delay - last_delay) * -1,
										track->GetTimeBase().GetNum(), track->GetTimeBase().GetDen(),
										_stat_recv_pkt_count[track_id],
										ov::Converter::ToSiString(_stat_recv_pkt_size[track_id], 1).CStr(),
										_stat_recv_pkt_size[track_id] / (uptime / 1000) * 8 / 1000);
		}

		ov::String stat_stream_str = "";

		stat_stream_str.AppendFormat("\n - MediaRouter Stream | id: %u, type: %s, name: %s/%s, uptime: %lldms, queue: %d, sync: %lldms",
									 _stream->GetId(),
									 _inout_type == MediaRouterStreamType::INBOUND ? "Inbound" : "Outbound",
									 _stream->GetApplicationInfo().GetName().CStr(),
									 _stream->GetName().CStr(),
									 (int64_t)uptime,
									 _packets_queue.Size(),
									 max_pts - min_pts);

		stat_track_str = stat_stream_str + stat_track_str;

		logtd("%s", stat_track_str.CStr());
	}
}

// @deprecated
void MediaRouteStream::DropNonDecodingPackets()
{
	std::map<MediaTrackId, int64_t> _pts_last;

	////////////////////////////////////////////////////////////////////////////////////
	// 1. Discover to the highest PTS value in the keyframe against packets on all tracks.

	std::vector<std::shared_ptr<MediaPacket>> tmp_packets_queue;
	int64_t base_pts = -1LL;

	while (true)
	{
		if (_packets_queue.IsEmpty())
			break;

		auto media_packet_ref = _packets_queue.Dequeue();
		if (media_packet_ref.has_value() == false)
			continue;

		auto &media_packet = media_packet_ref.value();

		if (media_packet->GetFlag() == MediaPacketFlag::Key)
		{
			if (base_pts < media_packet->GetPts())
			{
				base_pts = media_packet->GetPts();
				logtw("Discovered base PTS value track_id:%d, flags:%d, size:%d,  pts:%lld", (int32_t)media_packet->GetTrackId(), media_packet->GetFlag(), media_packet->GetDataLength(), base_pts);
			}
		}

		tmp_packets_queue.push_back(std::move(media_packet));
	}

	////////////////////////////////////////////////////////////////////////////////////
	// 2. Obtain the PTS values for all tracks close to the reference PTS.

	// <TrackId, <diff, Pts>>
	std::map<MediaTrackId, std::pair<int64_t, int64_t>> map_near_pts;

	for (auto it = tmp_packets_queue.begin(); it != tmp_packets_queue.end(); it++)
	{
		auto &media_packet = *it;

		if (media_packet->GetFlag() == MediaPacketFlag::Key)
		{
			MediaTrackId track_id = media_packet->GetTrackId();

			int64_t pts_diff = std::abs(media_packet->GetPts() - base_pts);

			auto it_near_pts = map_near_pts.find(track_id);

			if (it_near_pts == map_near_pts.end())
			{
				map_near_pts[track_id] = std::make_pair(pts_diff, media_packet->GetPts());
			}
			else
			{
				auto pair_value = it_near_pts->second;
				int64_t prev_pts_diff = pair_value.first;

				if (prev_pts_diff > pts_diff)
				{
					map_near_pts[track_id] = std::make_pair(pts_diff, media_packet->GetPts());
				}
			}
		}
	}

	////////////////////////////////////////////////////////////////////////////////////
	// 3. Drop all packets below PTS by all tracks

	uint32_t dropeed_packets = 0;
	for (auto it = tmp_packets_queue.begin(); it != tmp_packets_queue.end(); it++)
	{
		auto &media_packet = *it;

		if (media_packet->GetPts() < map_near_pts[media_packet->GetTrackId()].second)
		{
			dropeed_packets++;
			continue;
		}

		_packets_queue.Enqueue(std::move(media_packet));
	}
	tmp_packets_queue.clear();

	if (dropeed_packets > 0)
	{
		logtw("Number of dropped packets : %d", dropeed_packets);
	}
}

void MediaRouteStream::Push(std::shared_ptr<MediaPacket> media_packet)
{
	_packets_queue.Enqueue(std::move(media_packet));
}

std::shared_ptr<MediaPacket> MediaRouteStream::Pop()
{
	// Get Media Packet
	if (_packets_queue.IsEmpty())
	{
		return nullptr;
	}

	auto media_packet_ref = _packets_queue.Dequeue();
	if (media_packet_ref.has_value() == false)
	{
		return nullptr;
	}

	auto &media_packet = media_packet_ref.value();

	////////////////////////////////////////////////////////////////////////////////////
	// [ Calculating Packet Timestamp, Duration]

	// If there is no pts/dts value, do the same as pts/dts value.
	if (media_packet->GetDts() == -1LL)
	{
		media_packet->SetDts(media_packet->GetPts());
	}

	if (media_packet->GetPts() == -1LL)
	{
		media_packet->SetPts(media_packet->GetDts());
	}

	// Accumulate Packet duplication
	//	- 1) If the current packet does not have a Duration value then stashed.
	//	- 1) If packets stashed, calculate duration compared to the current packet timestamp.
	//	- 3) and then, the current packet stash.
	std::shared_ptr<MediaPacket> pop_media_packet = nullptr;

	if(GetInoutType() == MediaRouterStreamType::OUTBOUND) {
		auto it = _media_packet_stash.find(media_packet->GetTrackId());
		if (it == _media_packet_stash.end())
		{
			_media_packet_stash[media_packet->GetTrackId()] = std::move(media_packet);

			return nullptr;
		}

		pop_media_packet = std::move(it->second);

		int64_t duration = media_packet->GetDts() - pop_media_packet->GetDts();
		pop_media_packet->SetDuration(duration);

		_media_packet_stash[media_packet->GetTrackId()] = std::move(media_packet);
	}
	else {
		pop_media_packet = std::move(media_packet);
	}

	////////////////////////////////////////////////////////////////////////////////////
	// Bitstream format converting to standard format. and, parsing track informaion
	auto media_type = pop_media_packet->GetMediaType();
	auto track_id = pop_media_packet->GetTrackId();

	auto media_track = _stream->GetTrack(track_id);
	if (media_track == nullptr)
	{
		logte("Could not find the media track. track_id: %d, media_type: %s",
			  track_id,
			  (media_type == MediaType::Video) ? "video" : "audio");

		return nullptr;
	}


	switch (GetInoutType())
	{
		case MediaRouterStreamType::INBOUND: {
			// DumpPacket(pop_media_packet);

			if (!ProcessInboundStream(media_track, pop_media_packet))
			{
				return nullptr;
			}

			// If the parsing of track information is not complete, discard the packet.

			if (media_track->IsValid() == false)
			{
				return nullptr;
			}
		}
		break;

		case MediaRouterStreamType::OUTBOUND: {
			// DumpPacket(pop_media_packet, true);

			if (!ProcessOutboundStream(media_track, pop_media_packet))
			{
				return nullptr;
			}

			// If the parsing of track information is not complete, discard the packet.
			if (media_track->IsValid() == false)
			{
				return nullptr;
			}
		}
		break;

		default:
			break;
	}

	////////////////////////////////////////////////////////////////////////////////////
	// Detect abnormal increases in PTS.
	if (GetInoutType() == MediaRouterStreamType::INBOUND)
	{
		auto it = _pts_last.find(track_id);
		if (it != _pts_last.end())
		{
			int64_t ts_inc = pop_media_packet->GetPts() - _pts_last[track_id];
			int64_t ts_inc_ms = ts_inc * media_track->GetTimeBase().GetExpr();

			if (std::abs(ts_inc_ms) > PTS_CORRECT_THRESHOLD_MS)
			{
				if (!(media_track->GetCodecId() == cmn::MediaCodecId::Png || media_track->GetCodecId() == cmn::MediaCodecId::Jpeg))
				{
					logtw("Detected abnormal increased timestamp. track:%u last.pts: %lld, cur.pts: %lld, tb(%d/%d), diff: %lldms",
						  track_id, _pts_last[track_id],
						  pop_media_packet->GetPts(),
						  media_track->GetTimeBase().GetNum(),
						  media_track->GetTimeBase().GetDen(),
						  ts_inc_ms);
				}
			}
		}

		_pts_last[track_id] = pop_media_packet->GetPts();
		_dts_last[track_id] = pop_media_packet->GetDts();
	}

	////////////////////////////////////////////////////////////////////////////////////
	// Statistics
	UpdateStatistics(media_track, pop_media_packet);

	return pop_media_packet;
}

void MediaRouteStream::DumpPacket(
	std::shared_ptr<MediaPacket> &media_packet,
	bool dump)
{
	logtd("track_id(%d), type(%d), fmt(%d), flags(%d), pts(%lld) dts(%lld) dur(%d), size(%d)",
		  media_packet->GetTrackId(),
		  media_packet->GetPacketType(),
		  media_packet->GetBitstreamFormat(),
		  media_packet->GetFlag(),
		  media_packet->GetPts(),
		  media_packet->GetDts(),
		  media_packet->GetDuration(),
		  media_packet->GetDataLength());

	if (dump == true)
	{
		logtd("%s", media_packet->GetData()->Dump(128).CStr());
	}
}
