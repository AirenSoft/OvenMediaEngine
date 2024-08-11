//==============================================================================
//
//  MediaRouteStream
//
//  Created by Keukhan
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
// 		Append Decoder ParameterSets ->
// 		Changing the timestamp based on the timebase

// Outbound Stream process
//		Calculating packet duration ->
// 		Parsing Track Information ->
// 		Parsing Fragmentation Header ->
// 		Parsing Key-frame ->
// 		Append Decoder ParameterSets ->
// 		Changing the timestamp based on the timebase

#include "mediarouter_stream.h"

#include <base/ovlibrary/ovlibrary.h>
#include <modules/bitstream/aac/aac_adts.h>
#include <modules/bitstream/aac/aac_converter.h>
#include <modules/bitstream/aac/audio_specific_config.h>

#include <modules/bitstream/nalu/nal_stream_converter.h>
#include <modules/bitstream/h264/h264_decoder_configuration_record.h>
#include <modules/bitstream/h264/h264_common.h>
#include <modules/bitstream/h264/h264_parser.h>

#include <modules/bitstream/h265/h265_decoder_configuration_record.h>
#include <modules/bitstream/h265/h265_parser.h>
#include <modules/bitstream/nalu/nal_unit_fragment_header.h>
#include <modules/bitstream/opus/opus.h>
#include <modules/bitstream/vp8/vp8.h>
#include <modules/bitstream/mp3/mp3_parser.h>

#include "mediarouter_private.h"

#define PTS_CORRECT_THRESHOLD_MS 3000

using namespace cmn;

MediaRouteStream::MediaRouteStream(const std::shared_ptr<info::Stream> &stream)
	: _stream(stream),
	  _packets_queue(nullptr, 600)
{
	_inout_type = MediaRouterStreamType::UNKNOWN;

	_warning_count_bframe = 0;
	_warning_count_out_of_order = 0;

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
	_media_packet_stash.clear();

	_stat_recv_pkt_lpts.clear();
	_stat_recv_pkt_ldts.clear();

	_pts_last.clear();
}

std::shared_ptr<info::Stream> MediaRouteStream::GetStream()
{
	return _stream;
}

void MediaRouteStream::SetInoutType(MediaRouterStreamType inout_type)
{
	_inout_type = inout_type;

	auto urn = std::make_shared<info::ManagedQueue::URN>(
		_stream->GetApplicationInfo().GetVHostAppName(),
		_stream->GetName(),
		(_inout_type == MediaRouterStreamType::INBOUND) ? "imr" : "omr",
		"streamworker");
	_packets_queue.SetUrn(urn);
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
	// Clear queued packets
	_packets_queue.Clear();
	// Clear stashed Packets
	_media_packet_stash.clear();

	_are_all_tracks_parsed = false;

	_is_stream_prepared = false;
}

#include <base/ovcrypto/base_64.h>
bool MediaRouteStream::ProcessH264AVCCStream(std::shared_ptr<MediaTrack> &media_track, std::shared_ptr<MediaPacket> &media_packet)
{
	// Everytime : Convert to AnnexB, Make fragment header, Set keyframe flag, Append SPS/PPS nal units in front of IDR frame
	// one time : Parse track info from sps/pps and generate codec_extra_data

	if (media_packet->GetBitstreamFormat() != cmn::BitstreamFormat::H264_AVCC)
	{
		return false;
	}

	if (media_packet->GetPacketType() == cmn::PacketType::SEQUENCE_HEADER)
	{
		// Validation
		auto avc_config = std::make_shared<AVCDecoderConfigurationRecord>();

		if (avc_config->Parse(media_packet->GetData()) == false)
		{
			logte("Could not parse sequence header");
			return false;
		}

		media_track->SetWidth(avc_config->GetWidth());
		media_track->SetHeight(avc_config->GetHeight());

		media_track->SetDecoderConfigurationRecord(avc_config);
		
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

		std::shared_ptr<ov::Data> sps_nalu = nullptr, pps_nalu = nullptr;
		bool has_idr = false;
		bool has_sps = false;
		bool has_pps = false;
		bool has_aud = false;

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
				if (nal_header.GetNalUnitType() == H264NalUnitType::IdrSlice)
				{
					media_packet->SetFlag(MediaPacketFlag::Key);
					has_idr = true;
				}
				else if (nal_header.GetNalUnitType() == H264NalUnitType::Sps)
				{
					// logtd("[SPS] %s ", ov::Base64::Encode(nalu).CStr());
					has_sps = true;
					if (sps_nalu == nullptr)
					{
						sps_nalu = std::make_shared<ov::Data>(nalu->GetDataAs<uint8_t>(), nalu->GetLength());
					}
				}
				else if (nal_header.GetNalUnitType() == H264NalUnitType::Pps)
				{
					// logtd("[PPS] %s ", ov::Base64::Encode(nalu).CStr());
					has_pps = true;
					if (pps_nalu == nullptr)
					{
						pps_nalu = std::make_shared<ov::Data>(nalu->GetDataAs<uint8_t>(), nalu->GetLength());
					}					
				}
				else if (nal_header.GetNalUnitType() == H264NalUnitType::Aud)
				{
					has_aud = true;
				}
				else if (nal_header.GetNalUnitType() == H264NalUnitType::FillerData)
				{
					// no need to maintain filler data
					continue;
				}
			}

			converted_data->Append(START_CODE, sizeof(START_CODE));
			nalu_offset += START_CODE_LEN;

			fragment_header.AddFragment(nalu_offset, nalu->GetLength());

			converted_data->Append(nalu);
			nalu_offset += nalu->GetLength();
		}

		media_packet->SetFragHeader(&fragment_header);
		media_packet->SetData(converted_data);
		media_packet->SetBitstreamFormat(cmn::BitstreamFormat::H264_ANNEXB);
		media_packet->SetPacketType(cmn::PacketType::NALU);

		// Update DecoderConfigurationRecord
		if(sps_nalu != nullptr && pps_nalu != nullptr)
		{
			auto new_avc_config = std::make_shared<AVCDecoderConfigurationRecord>();

			new_avc_config->AddSPS(sps_nalu);
			new_avc_config->AddPPS(pps_nalu);

			if (new_avc_config->IsValid())
			{
				auto old_avc_config = std::static_pointer_cast<AVCDecoderConfigurationRecord>(media_track->GetDecoderConfigurationRecord());

				if (media_track->IsValid() == false || old_avc_config == nullptr || old_avc_config->Equals(new_avc_config) == false)
				{
					media_track->SetWidth(new_avc_config->GetWidth());
					media_track->SetHeight(new_avc_config->GetHeight());
					media_track->SetDecoderConfigurationRecord(new_avc_config);
				}
			}
			else 
			{
				auto stream_info = GetStream();
				logtw("Failed to make AVCDecoderConfigurationRecord of %s/%s/%s track", stream_info->GetApplicationName(), stream_info->GetName().CStr(), media_track->GetVariantName().CStr());
			}
		}

		// Insert SPS/PPS if there are no SPS/PPS nal units before IDR frame.
		if (has_idr == true && (has_sps == false || has_pps == false))
		{
			if (InsertH264SPSPPSAnnexB(media_track, media_packet, !has_aud) == false)
			{
				auto stream_info = GetStream();
				logtw("Failed to insert SPS/PPS before IDR frame in %s/%s/%s track", stream_info->GetApplicationName(), stream_info->GetName().CStr(), media_track->GetVariantName().CStr());
			}
		}
		else if (has_aud == false)
		{
			if (InsertH264AudAnnexB(media_track, media_packet) == false)
			{
				logtw("Failed to insert AUD before IDR frame in %s/%s/%s track", GetStream()->GetApplicationName(), GetStream()->GetName().CStr(), media_track->GetVariantName().CStr());
			}
		}

		return true;
	}

	return false;
}

bool MediaRouteStream::ProcessH264AnnexBStream(std::shared_ptr<MediaTrack> &media_track, std::shared_ptr<MediaPacket> &media_packet)
{
	// Everytime : Make fragment header, Set keyframe flag, Append SPS/PPS nal units in front of IDR frame
	// one time : Parse track info from sps/pps and generate codec_extra_data
	std::shared_ptr<ov::Data> sps_nalu = nullptr, pps_nalu = nullptr;

	FragmentationHeader fragment_header;
	size_t offset = 0, offset_length = 0;
	auto bitstream = media_packet->GetData()->GetDataAs<uint8_t>();
	auto bitstream_length = media_packet->GetData()->GetLength();
	bool has_sps = false, has_pps = false, has_idr = false, has_aud = false;

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

		fragment_header.AddFragment(offset, offset_length);

		H264NalUnitHeader nal_header;
		if (H264Parser::ParseNalUnitHeader(bitstream + offset, offset_length, nal_header) == false)
		{
			logte("Could not parse H264 Nal unit header");
			return false;
		}
		
		if (nal_header.GetNalUnitType() == H264NalUnitType::Sps)
		{
			has_sps = true;
			
			if (sps_nalu == nullptr)
			{
				sps_nalu = std::make_shared<ov::Data>(bitstream + offset, offset_length);
			}
		}
		else if (nal_header.GetNalUnitType() == H264NalUnitType::Pps)
		{
			has_pps = true;

			if (pps_nalu == nullptr)
			{
				pps_nalu = std::make_shared<ov::Data>(bitstream + offset, offset_length);
			}
		}
		else if (nal_header.GetNalUnitType() == H264NalUnitType::IdrSlice)
		{
			has_idr = true;
			media_packet->SetFlag(MediaPacketFlag::Key);
		}
		else if (nal_header.GetNalUnitType() == H264NalUnitType::Aud)
		{
			has_aud = true;
		}
		else if (nal_header.GetNalUnitType() == H264NalUnitType::FillerData)
		{
			//TODO(Getroot): It is better to remove filler data.
		}

		// Last NalU
		if (pos == -1)
		{
			break;
		}

		offset += pos;
	}
	media_packet->SetFragHeader(&fragment_header);

	// Update DecoderConfigurationRecord
	if (sps_nalu != nullptr && pps_nalu != nullptr)
	{
		auto new_avc_config = std::make_shared<AVCDecoderConfigurationRecord>();

		new_avc_config->AddSPS(sps_nalu);
		new_avc_config->AddPPS(pps_nalu);

		if (new_avc_config->IsValid())
		{
			auto old_avc_config = std::static_pointer_cast<AVCDecoderConfigurationRecord>(media_track->GetDecoderConfigurationRecord());

			if (media_track->IsValid() == false || old_avc_config == nullptr || old_avc_config->Equals(new_avc_config) == false)
			{
				media_track->SetWidth(new_avc_config->GetWidth());
				media_track->SetHeight(new_avc_config->GetHeight());
				media_track->SetDecoderConfigurationRecord(new_avc_config);
			}
		}
		else
		{
			logtw("Failed to make AVCDecoderConfigurationRecord of %s/%s/%s track", GetStream()->GetApplicationName(), GetStream()->GetName().CStr(), media_track->GetVariantName().CStr());
		}
	}

	// Insert SPS/PPS if there are no SPS/PPS nal units before IDR frame.
	if (has_idr == true && media_track->IsValid() && (has_sps == false || has_pps == false))
	{
		if (InsertH264SPSPPSAnnexB(media_track, media_packet, !has_aud) == false)
		{
			logtw("Failed to insert SPS/PPS before IDR frame in %s/%s/%s track", GetStream()->GetApplicationName(), GetStream()->GetName().CStr(), media_track->GetVariantName().CStr());
		}
	}
	else if (has_aud == false)
	{
		if (InsertH264AudAnnexB(media_track, media_packet) == false)
		{
			logtw("Failed to insert AUD before IDR frame in %s/%s/%s track", GetStream()->GetApplicationName(), GetStream()->GetName().CStr(), media_track->GetVariantName().CStr());
		}
	}

	return true;
}

bool MediaRouteStream::InsertH264SPSPPSAnnexB(std::shared_ptr<MediaTrack> &media_track, std::shared_ptr<MediaPacket> &media_packet, bool need_aud)
{
	if (media_track->IsValid() == false)
	{
		return false;
	}

	// Get AVC Decoder Configuration Record
	auto avc_config = std::static_pointer_cast<AVCDecoderConfigurationRecord>(media_track->GetDecoderConfigurationRecord());
	if (avc_config == nullptr)
	{
		return false;
	}

	auto data = media_packet->GetData();
	auto frag_header = media_packet->GetFragHeader();

	const uint8_t START_CODE[4] = {0x00, 0x00, 0x00, 0x01};
	const size_t START_CODE_LEN = sizeof(START_CODE);

	auto [sps_pps_annexb, sps_pps_frag_header] = avc_config->GetSpsPpsAsAnnexB(START_CODE_LEN, need_aud);
	if (sps_pps_annexb == nullptr)
	{
		return false;
	}

	// new media packet
	auto processed_data = std::make_shared<ov::Data>(data->GetLength() + 1024);

	// copy sps/pps first
	processed_data->Append(sps_pps_annexb);
	// and then copy original data
	processed_data->Append(data);

	// Update fragment header
	FragmentationHeader updated_frag_header;
	updated_frag_header.fragmentation_offset = sps_pps_frag_header.fragmentation_offset;
	updated_frag_header.fragmentation_length = sps_pps_frag_header.fragmentation_length;

	// Existing fragment header offset because SPS/PPS was inserted at front
	auto offset_offset = updated_frag_header.fragmentation_offset.back() + updated_frag_header.fragmentation_length.back();
	for (size_t i = 0; i < frag_header->fragmentation_offset.size(); i++)
	{
		size_t updated_offset = frag_header->fragmentation_offset[i] + offset_offset;
		updated_frag_header.AddFragment(updated_offset, frag_header->fragmentation_length[i]);
	}

	media_packet->SetFragHeader(&updated_frag_header);
	media_packet->SetData(processed_data);

	return true;
}

bool MediaRouteStream::InsertH264AudAnnexB(std::shared_ptr<MediaTrack> &media_track, std::shared_ptr<MediaPacket> &media_packet)
{
	if (media_track->IsValid() == false)
	{
		return false;
	}

	auto data = media_packet->GetData();
	auto frag_header = media_packet->GetFragHeader();

	const uint8_t START_CODE[4] = {0x00, 0x00, 0x00, 0x01};
	const size_t START_CODE_LEN = sizeof(START_CODE);

	// new media packet
	auto processed_data = std::make_shared<ov::Data>(data->GetLength() + 1024);

	// copy AUD first
	const uint8_t AUD[2] = {0x09, 0xf0};
	processed_data->Append(START_CODE, sizeof(START_CODE));
	processed_data->Append(AUD, sizeof(AUD));

	// and then copy original data
	processed_data->Append(data);

	// Update fragment header
	FragmentationHeader updated_frag_header;
	updated_frag_header.AddFragment(START_CODE_LEN, sizeof(AUD));

	// Existing fragment header offset because AUD was inserted at front
	auto offset_offset = sizeof(AUD) + START_CODE_LEN;
	for (size_t i = 0; i < frag_header->fragmentation_offset.size(); i++)
	{
		size_t updated_offset = frag_header->fragmentation_offset[i] + offset_offset;
		updated_frag_header.AddFragment(updated_offset, frag_header->fragmentation_length[i]);
	}

	media_packet->SetFragHeader(&updated_frag_header);
	media_packet->SetData(processed_data);

	return true;

}

bool MediaRouteStream::ProcessAACRawStream(std::shared_ptr<MediaTrack> &media_track, std::shared_ptr<MediaPacket> &media_packet)
{
	media_packet->SetFlag(MediaPacketFlag::Key);
	// everytime : Convert to ADTS
	// one time : Parse sequence header
	if (media_packet->GetPacketType() == cmn::PacketType::SEQUENCE_HEADER)
	{
		if (media_track->IsValid() == false)
		{
			// Validation
			auto audio_config = std::make_shared<AudioSpecificConfig>();
			if (audio_config->Parse(media_packet->GetData()) == false)
			{
				logte("aac sequence header paring error");
				return false;
			}

			media_track->SetSampleRate(audio_config->SamplerateNum());
			media_track->GetChannel().SetLayout(audio_config->Channel() == 1 ? AudioChannel::Layout::LayoutMono : AudioChannel::Layout::LayoutStereo);
			
			media_track->SetDecoderConfigurationRecord(audio_config);
		}

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

		auto audio_config = std::static_pointer_cast<AudioSpecificConfig>(media_track->GetDecoderConfigurationRecord());
		if (audio_config == nullptr)
		{
			logte("Failed to get audio specific config.");
			return false;
		}

		// Convert to adts (raw aac data should be 1 frame)
		auto adts_data = AacConverter::ConvertRawToAdts(media_packet->GetData(), audio_config);
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

	// Make AudioSpecificConfig from ADTS Header
	AACAdts adts;
	if (AACAdts::Parse(media_packet->GetData()->GetDataAs<uint8_t>(), media_packet->GetDataLength(), adts) == false)
	{
		logte("Could not parse AAC ADTS header");
		return false;
	}

	auto audio_config = std::make_shared<AudioSpecificConfig>();
	audio_config->SetOjbectType(adts.Profile());
	audio_config->SetSamplingFrequency(adts.Samplerate());
	audio_config->SetChannel(adts.ChannelConfiguration());

	media_track->SetSampleRate(audio_config->SamplerateNum());
	media_track->GetChannel().SetLayout(audio_config->Channel() == 1 ? AudioChannel::Layout::LayoutMono : AudioChannel::Layout::LayoutStereo);

	media_track->SetDecoderConfigurationRecord(audio_config);

	return true;
}

bool MediaRouteStream::ProcessH265AnnexBStream(std::shared_ptr<MediaTrack> &media_track, std::shared_ptr<MediaPacket> &media_packet)
{
	// Everytime : Generate fragmentation header, Check key frame
	// One time : Parse SPS and Set width/height (track information)

	// TODO : Append SPS/PPS nal units in front of IDR frame

	std::shared_ptr<HEVCDecoderConfigurationRecord> hevc_config = nullptr;
	FragmentationHeader fragment_header;

	auto bitstream = media_packet->GetData()->GetDataAs<uint8_t>();
	auto bitstream_length = media_packet->GetData()->GetLength();
	
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

		fragment_header.AddFragment(offset, offset_length);

		H265NalUnitHeader header;
		if (H265Parser::ParseNalUnitHeader(bitstream + offset, H265_NAL_UNIT_HEADER_SIZE, header) == false)
		{
			logte("Could not parse H265 Nal unit header");
			return false;
		}

		// Key Frame
		if (header.GetNalUnitType() == H265NALUnitType::BLA_W_LP ||
			header.GetNalUnitType() == H265NALUnitType::BLA_W_RADL ||
			header.GetNalUnitType() == H265NALUnitType::BLA_N_LP ||
			header.GetNalUnitType() == H265NALUnitType::IDR_W_RADL ||
			header.GetNalUnitType() == H265NALUnitType::IDR_N_LP ||
			header.GetNalUnitType() == H265NALUnitType::CRA_NUT ||
			header.GetNalUnitType() == H265NALUnitType::IRAP_VCL22 ||
			header.GetNalUnitType() == H265NALUnitType::IRAP_VCL23)
		{
			media_packet->SetFlag(MediaPacketFlag::Key);
		}

		// Track info
		if (media_track->IsValid() == false)
		{
			if (hevc_config == nullptr)
			{
				hevc_config = std::make_shared<HEVCDecoderConfigurationRecord>();
			}

			if (header.GetNalUnitType() == H265NALUnitType::VPS ||
				header.GetNalUnitType() == H265NALUnitType::SPS ||
				header.GetNalUnitType() == H265NALUnitType::PPS)
			{
				auto nal_unit = std::make_shared<ov::Data>(bitstream + offset, offset_length);
				hevc_config->AddNalUnit(header.GetNalUnitType(), nal_unit);
			}
		}

		// Last NalU
		if (pos == -1)
		{
			break;
		}

		offset += pos;
	}
	media_packet->SetFragHeader(&fragment_header);

	// Update DecoderConfigurationRecord
	if(hevc_config && hevc_config->IsValid())
	{
		auto old_hevc_config = std::static_pointer_cast<HEVCDecoderConfigurationRecord>(media_track->GetDecoderConfigurationRecord());

		if (old_hevc_config == nullptr || old_hevc_config->Equals(hevc_config) == false)
		{
			media_track->SetWidth(hevc_config->GetWidth());
			media_track->SetHeight(hevc_config->GetHeight());
			media_track->SetDecoderConfigurationRecord(hevc_config);
		}
	}

	return true;
}

bool MediaRouteStream::ProcessH265HVCCStream(std::shared_ptr<MediaTrack> &media_track, std::shared_ptr<MediaPacket> &media_packet)
{
	if (media_packet->GetBitstreamFormat() != cmn::BitstreamFormat::HVCC)
	{
		return false;
	}

	if (media_packet->GetPacketType() == cmn::PacketType::SEQUENCE_HEADER)
	{
		// Validation
		auto hevc_config = std::make_shared<HEVCDecoderConfigurationRecord>();

		if (hevc_config->Parse(media_packet->GetData()) == false)
		{
			logte("Could not parse sequence header");
			return false;
		}

		media_track->SetWidth(hevc_config->GetWidth());
		media_track->SetHeight(hevc_config->GetHeight());

		media_track->SetDecoderConfigurationRecord(hevc_config);

		return false;
	}
	else if (media_packet->GetPacketType() == cmn::PacketType::NALU)
	{
		auto converted_data = NalStreamConverter::ConvertXvccToAnnexb(media_packet->GetData());

		if (converted_data == nullptr)
		{
			logte("Failed to convert HVCC to AnnexB");
			return false;
		}

		media_packet->SetData(converted_data);
		media_packet->SetBitstreamFormat(cmn::BitstreamFormat::H265_ANNEXB);
		media_packet->SetPacketType(cmn::PacketType::NALU);

		// Because AVC is used in webrtc, there is a function to insert it if sps/pps is not present in the keyframe NAL, but HEVC is omitted because it is not used in webrtc. It will be added in the future if needed.
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

	// TODO(Getroot) : In VP8, there is no need to know whether it is the current keyframe. So it doesn't parse every time.
	// However, if this is needed in the future, VP8Parser writes and applies a low-cost code that only determines whether or not it is a keyframe.
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

bool MediaRouteStream::ProcessMP3Stream(std::shared_ptr<MediaTrack> &media_track, std::shared_ptr<MediaPacket> &media_packet)
{
	// One time : parse samplerate, channel
	if (media_track->IsValid() == true)
	{
		return true;
	}

	MP3Parser parser;
	if (MP3Parser::Parse(media_packet->GetData()->GetDataAs<uint8_t>(), media_packet->GetDataLength(), parser) == false)
	{
		logte("Could not parse MP3 header");
		return false;
	}

	logti("MP3Parser : %s", parser.GetInfoString().CStr());

	media_track->SetSampleRate(parser.GetSampleRate());
	media_track->SetBitrateByMeasured(parser.GetBitrate());
	media_track->GetChannel().SetLayout((parser.GetChannelCount() == 1) ? (AudioChannel::Layout::LayoutMono) : (AudioChannel::Layout::LayoutStereo));

	return true;
}

// H264 : AVCC -> AnnexB, Add SPS/PPS in front of IDR frame
// H265 : 
// AAC : Raw -> ADTS
bool MediaRouteStream::NormalizeMediaPacket(std::shared_ptr<MediaTrack> &media_track, std::shared_ptr<MediaPacket> &media_packet)
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
		case cmn::BitstreamFormat::HVCC:
			result = ProcessH265HVCCStream(media_track, media_packet);
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
		case cmn::BitstreamFormat::MP3:
			result = ProcessMP3Stream(media_track, media_packet); 
			break;
		case cmn::BitstreamFormat::ID3v2:
		case cmn::BitstreamFormat::OVEN_EVENT:
			result = true;
			break;
		case cmn::BitstreamFormat::JPEG:
		case cmn::BitstreamFormat::PNG:
		{
			if (GetInoutType()  == MediaRouterStreamType::OUTBOUND)
			{
				result = true;
			}
			break;
		}
		
		case cmn::BitstreamFormat::AAC_LATM:
		case cmn::BitstreamFormat::Unknown:
		default:
			logte("Bitstream not supported by inbound");
			break;
	}

	return result;
}

// Check whether the information extraction for all tracks has been completed.
bool MediaRouteStream::AreAllTracksReady()
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

		// If the track is an outbound track, it is necessary to check the quality.
		if (track->HasQualityMeasured() == false)
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
		case cmn::BitstreamFormat::HVCC:
			if (_warning_count_bframe < 10)
			{
				if (media_track->GetTotalFrameCount() > 0 && _stat_recv_pkt_lpts[track_id] > media_packet->GetPts())
				{
					media_track->SetHasBframes(true);
				}

				// Display a warning message that b-frame exists
				if (media_track->HasBframes() == true)
				{
					logtw("[%s/%s(%u)] b-frame has been detected in the %u track of %s stream",
						  _stream->GetApplicationInfo().GetVHostAppName().CStr(),
						  _stream->GetName().CStr(),
						  _stream->GetId(),
						  track_id,
						  _inout_type == MediaRouterStreamType::INBOUND ? "inbound" : "outbound");

					_warning_count_bframe++;
				}
			}
			break;
		default:
			break;
	}

	_stat_recv_pkt_lpts[track_id] = media_packet->GetPts();
	_stat_recv_pkt_ldts[track_id] = media_packet->GetDts();

	if (_stop_watch.IsElapsed(30000) && _stop_watch.Update())
	{
		// Uptime
		int64_t uptime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - _stat_start_time).count();

		int64_t min_pts = -1LL;
		int64_t max_pts = -1LL;

		ov::String stat_track_str = "";

		for (const auto &[track_id, track] : _stream->GetTracks())
		{
			int64_t rescaled_last_pts = (int64_t)((double)(_stat_recv_pkt_lpts[track_id] * 1000) * track->GetTimeBase().GetExpr());
			
			// Time difference in pts values relative to uptime
			int64_t last_delay = uptime - rescaled_last_pts;

			stat_track_str.AppendFormat("\n\ttrack:%11u, type: %5s, codec: %4s(%d,%s), pts: %lldms, dly: %5lldms, tb: %d/%5d, pkt_cnt: %6lld, pkt_siz: %sB, bps: %s/%s",
										track_id,
										GetMediaTypeString(track->GetMediaType()).CStr(),
										::StringFromMediaCodecId(track->GetCodecId()).CStr(),
										track->GetCodecId(),
										track->IsBypass() ? "Passthrough" : GetStringFromCodecModuleId(track->GetCodecModuleId()).CStr(),
										rescaled_last_pts,
										last_delay,
										track->GetTimeBase().GetNum(), track->GetTimeBase().GetDen(),
										track->GetTotalFrameCount(),
										ov::Converter::ToSiString(track->GetTotalFrameBytes(), 1).CStr(),
										ov::Converter::BitToString(track->GetBitrateByMeasured()).CStr(), ov::Converter::BitToString(track->GetBitrateByConfig()).CStr());

			if(track->GetMediaType() == MediaType::Data)
			{
				continue;
			}

			if (track->GetMediaType() == MediaType::Video)
			{
				stat_track_str.AppendFormat(", fps: %.2f/%.2f",
											track->GetFrameRateByMeasured(), track->GetFrameRate());
				stat_track_str.AppendFormat(", kint: %.2f/%d/%s",
											track->GetKeyFrameIntervalByMeasured(),
											track->GetKeyFrameInterval(),
											cmn::GetKeyFrameIntervalTypeToString(track->GetKeyFrameIntervalTypeByConfig()).CStr());
			}

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
		}

		ov::String stat_stream_str = "";

		stat_stream_str.AppendFormat("\n - Stream | id: %u, type: %s, name: %s/%s, uptime: %lldms, queue: %d, sync: %lldms",
									 _stream->GetId(),
									 _inout_type == MediaRouterStreamType::INBOUND ? "Inbound" : "Outbound",
									 _stream->GetApplicationInfo().GetVHostAppName().CStr(),
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

	uint32_t dropped_packets = 0;
	for (auto it = tmp_packets_queue.begin(); it != tmp_packets_queue.end(); it++)
	{
		auto &media_packet = *it;

		if (media_packet->GetPts() < map_near_pts[media_packet->GetTrackId()].second)
		{
			dropped_packets++;
			continue;
		}

		_packets_queue.Enqueue(std::move(media_packet));
	}
	tmp_packets_queue.clear();

	if (dropped_packets > 0)
	{
		logtw("Number of dropped packets : %d", dropped_packets);
	}
}

void MediaRouteStream::Push(const std::shared_ptr<MediaPacket> &media_packet)
{
	_packets_queue.Enqueue(media_packet, media_packet->IsHighPriority());
}

std::shared_ptr<MediaPacket> MediaRouteStream::PopAndNormalize()
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

	if (media_packet->GetMediaType() == MediaType::Data)
	{
		return media_packet;
	}

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

	if ((GetInoutType() == MediaRouterStreamType::OUTBOUND) &&	
		(media_packet->GetDuration()) <= 0 &&
		// The packet duration recalculation applies only to video and audio types.
		(media_packet->GetMediaType() == MediaType::Video || media_packet->GetMediaType() == MediaType::Audio))
	{
		auto it = _media_packet_stash.find(media_packet->GetTrackId());
		if (it == _media_packet_stash.end())
		{
			_media_packet_stash[media_packet->GetTrackId()] = std::move(media_packet);

			return nullptr;
		}

		pop_media_packet = std::move(it->second);

		// [#743] Recording and HLS packetizing are failing due to non-monotonically increasing dts.
		// So, the code below is a temporary measure to avoid this problem. A more fundamental solution should be considered.
		if (pop_media_packet->GetDts() >= media_packet->GetDts())
		{
			if (_warning_count_out_of_order++ < 10)
			{
				logtw("[%s/%s] Detected out of order DTS of packet. track_id:%d dts:%lld->%lld",
					  _stream->GetApplicationName(), _stream->GetName().CStr(), pop_media_packet->GetTrackId(), pop_media_packet->GetDts(), media_packet->GetDts());
			}

			// If a packet has entered this function, it's a really weird stream.
			// It must be seen that the order of the packets is jumbled.
			media_packet->SetPts(pop_media_packet->GetPts() + 1);
			media_packet->SetDts(pop_media_packet->GetDts() + 1);
		}  // end of temporary measure code

		int64_t duration = media_packet->GetDts() - pop_media_packet->GetDts();
		pop_media_packet->SetDuration(duration);

		_media_packet_stash[media_packet->GetTrackId()] = std::move(media_packet);
	}
	else
	{
		pop_media_packet = std::move(media_packet);

		// The packet duration of data type is always 0.
		if(pop_media_packet->GetMediaType() == MediaType::Data)
		{
			pop_media_packet->SetDuration(0);
		}
	}

	////////////////////////////////////////////////////////////////////////////////////
	// Bitstream format converting to standard format. and, parsing track information
	auto media_type = pop_media_packet->GetMediaType();
	auto track_id = pop_media_packet->GetTrackId();

	auto media_track = _stream->GetTrack(track_id);
	if (media_track == nullptr)
	{
		logte("Could not find the media track. track_id: %d, media_type: %s",
			  track_id,
			  GetMediaTypeString(media_type).CStr());

		return nullptr;
	}
	
	// Convert bitstream format and normalize (e.g. Add SPS/PPS to head of H264 IDR frame)
	if (NormalizeMediaPacket(media_track, pop_media_packet) == false)
	{
		return nullptr;
	}

	if (GetInoutType() == MediaRouterStreamType::OUTBOUND &&
		pop_media_packet->GetDuration() < 0)
	{
		logtw("[%s/%s] found invalid duration of packet. We need to find the cause of the incorrect Duration.", _stream->GetApplicationName(), _stream->GetName().CStr());
		DumpPacket(pop_media_packet, false);

		return nullptr;
	}

	media_track->OnFrameAdded(pop_media_packet);

	// Detect abnormal increases in PTS.
	if (GetInoutType() == MediaRouterStreamType::INBOUND)
	{
		DetectAbnormalPackets(pop_media_packet);
	}

	// Statistics
	UpdateStatistics(media_track, pop_media_packet);

	// Mirror Buffer
	_mirror_buffer.emplace_back(std::make_shared<MirrorBufferItem>(pop_media_packet));
	// Delete old packets
	for (auto it = _mirror_buffer.begin(); it != _mirror_buffer.end();)
	{
		auto item = *it;
		if (item->GetElapsedMilliseconds() > 3000)
		{
			it = _mirror_buffer.erase(it);
		}
		else
		{
			++it;
		}
	}

	return pop_media_packet;
}

std::vector<std::shared_ptr<MediaRouteStream::MirrorBufferItem>> MediaRouteStream::GetMirrorBuffer()
{
	return _mirror_buffer;
}

void MediaRouteStream::DetectAbnormalPackets(std::shared_ptr<MediaPacket> &packet)
{
	auto track_id = packet->GetTrackId();
	auto it = _pts_last.find(track_id);
	if (it != _pts_last.end())
	{
		auto media_track = _stream->GetTrack(track_id);
		if (!media_track)
			return;

		int64_t ts_ms = packet->GetPts() * media_track->GetTimeBase().GetExpr();
		int64_t ts_diff_ms = ts_ms - _pts_last[track_id];

		if (std::abs(ts_diff_ms) > PTS_CORRECT_THRESHOLD_MS)
		{
			if (IsImageCodec(media_track->GetCodecId()) == false)
			{
				logtw("[%s/%s(%u)] Detected abnormal increased timestamp. track:%u last.pts: %lld, cur.pts: %lld, tb(%d/%d), diff: %lldms",
					  _stream->GetApplicationInfo().GetVHostAppName().CStr(),
					  _stream->GetName().CStr(),
					  _stream->GetId(),
					  track_id,
					  _pts_last[track_id],
					  packet->GetPts(),
					  media_track->GetTimeBase().GetNum(),
					  media_track->GetTimeBase().GetDen(),
					  ts_diff_ms);
			}
		}

		_pts_last[track_id] = ts_ms;
	}
}

void MediaRouteStream::DumpPacket(
	std::shared_ptr<MediaPacket> &media_packet,
	bool dump)
{
	double expr = GetStream()->GetTrack(media_packet->GetTrackId())->GetTimeBase().GetExpr() * 1000;
	logti("[%s/%s] track_id(%2d), type(%2d), fmt(%2d), flags(%d), pts(%10lld) dts(%10lld) dur(%5d/%5d), size(%d)",
		  _stream->GetApplicationName(),
		  _stream->GetName().CStr(),
		  media_packet->GetTrackId(),
		  media_packet->GetPacketType(),
		  media_packet->GetBitstreamFormat(),
		  media_packet->GetFlag(),
		  (int64_t)((double)media_packet->GetPts() * expr),
		  (int64_t)((double)media_packet->GetDts() * expr),
		  (int32_t)((double)media_packet->GetDuration() * expr),
		  (int32_t)media_packet->GetDuration(),
		  media_packet->GetDataLength());

	if (dump == true)
	{
		logtd("%s", media_packet->GetData()->Dump(128).CStr());
	}
}
