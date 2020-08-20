
#include "aac_latm_to_adts.h"
#include <modules/bitstream/aac/aac_specific_config.h>


#define OV_LOG_TAG "AACLatmToAdts"

bool AACLatmToAdts::GetExtradata(const common::PacketType type, const std::shared_ptr<ov::Data> &data, std::vector<uint8_t> &extradata)
{
	if(type == common::PacketType::SEQUENCE_HEADER)
	{
		AACSpecificConfig config;
		if(!AACSpecificConfig::Parse((data->GetDataAs<uint8_t>()), data->GetLength(), config))
		{
			logte("aac sequence paring error"); 
			return false;
		}
		
		extradata = config.Serialize();

		logtd("%s\r\n%d", config.GetInfoString().CStr(), extradata.size());;

		return true;   
	}

	return false;
}

bool AACLatmToAdts::Convert(const std::shared_ptr<MediaPacket> &packet, const std::vector<uint8_t> &extradata)
{
	common::BitstreamFormat bitstream_format = packet->GetBitstreamFormat();
	common::PacketType pkt_type = packet->GetPacketType();
	if(bitstream_format == common::BitstreamFormat::AAC_ADTS)
	{
		return true;
	}

	if(AACLatmToAdts::Convert(pkt_type, packet->GetData(), extradata) == false)
	{
		return false;
	}

	packet->SetBitstreamFormat(common::BitstreamFormat::AAC_ADTS);
	packet->SetPacketType(common::PacketType::RAW);

	return true;
}

/*
	ADTS
	Unlike the ADIF header, ADTS (Audio Data Transport Stream) headers are present before each AAC raw_data_block or block of 2 to 4 raw_data_blocks. Until the MPEG revision from Dec 2002 for MPEG-4 AAC ADTS headers, this was basically the same as a MP3 header, except that the emphasis field was not present for MPEG-2 AAC, only for MPEG-4 AAC.
	Now the emphasis field (2 bits) has been abandoned completely, and thus MPEG-4 and MPEG-2 AAC ADTS headers are exactly the same except for the Object Type ID flag (MPEG-2 or MPEG-4). See also the Wiki page about MP4, because this is important when extracting MPEG-2 AAC files from a MP4 container with mp4creator or creating MP4 files from PsyTEL AAC encodings with this tool.

	The ADTS header has the following fields:
	Field name   Field size in bits   Comment
	ADTS Fixed header: these don't change from frame to frame
	syncword   12   always: '111111111111'
	ID   1   0: MPEG-4, 1: MPEG-2
	layer   2   always: '00'
	protection_absent   1   
	profile   2   
	sampling_frequency_index   4   
	private_bit   1   
	channel_configuration   3   
	original/copy   1   
	home   1   
	ADTS Variable header: these can change from frame to frame
	copyright_identification_bit   1   
	copyright_identification_start   1   
	aac_frame_length   13   length of the frame including header (in bytes)
	adts_buffer_fullness   11   0x7FF indicates VBR
	no_raw_data_blocks_in_frame   2   
	ADTS Error check
	crc_check   16   only if protection_absent == 0
	After that come (no_raw_data_blocks_in_frame+1) raw_data_blocks.
	Some elaborations:
	profile
	bits   ID == 1 (MPEG-2 profile)   ID == 0 (MPEG-4 Object type)
	00 (0)   Main profile   AAC MAIN
	01 (1)   Low Complexity profile (LC)   AAC LC
	10 (2)   Scalable Sample Rate profile (SSR)   AAC SSR
	11 (3)   (reserved)   AAC LTP
*/
bool AACLatmToAdts::Convert(const common::PacketType type, const std::shared_ptr<ov::Data> &data, const std::vector<uint8_t> &extradata)
{
	auto annexb_data = std::make_shared<ov::Data>();
	annexb_data->Clear();

	if(type == common::PacketType::SEQUENCE_HEADER)
	{
		// There is no need to convert.
		return false;
	}
	else if(type == common::PacketType::RAW)
	{
		int16_t aac_raw_length = data->GetLength();

		//Get the AACSecificConfig value from extradata;
		AACSpecificConfig aac_specific_config;
		aac_specific_config.Deserialize(extradata);

		uint8_t aac_profile = (uint8_t)aac_specific_config.GetAacProfile();
		uint8_t aac_sample_rate = (uint8_t)aac_specific_config.SamplingFrequency();
		uint8_t aac_channels = (uint8_t)aac_specific_config.Channel();
		int16_t aac_frame_length = aac_raw_length + 7;
		uint8_t aac_fixed_header[7];
		uint8_t *pp = aac_fixed_header;

		*pp++ = 0xff;
		*pp++ = 0xf1;
		*pp++ = ((aac_profile << 6) & 0xc0) | ((aac_sample_rate << 2) & 0x3c) | ((aac_channels >> 2) & 0x01);
		*pp++ = ((aac_channels << 6) & 0xc0) | ((aac_frame_length >> 11) & 0x03);
		*pp++ = aac_frame_length >> 3;
		*pp++ = (aac_frame_length << 5) & 0xe0;
		*pp++ = 0xfc;

		annexb_data->Append(aac_fixed_header, sizeof(aac_fixed_header));
		annexb_data->Append(data->Subdata(0, aac_raw_length));
	}

	data->Clear();

	if(annexb_data->GetLength() > 0)
	{
		data->Append(annexb_data);
	}

	return true;
}
