
#include "aac_latm_to_adts.h"
#include <modules/bitstream/aac/aac_specific_config.h>
#include <base/ovlibrary/ovlibrary.h>

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
		
		 config.Serialize(extradata);

		return true;   
	}

	return false;
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
		if(extradata.size() > 0)
		{
			AACSpecificConfig aac_specific_config;
			AACSpecificConfig::Parse(&extradata.front(), extradata.size(), aac_specific_config);

			uint8_t aac_profile = (uint8_t)aac_specific_config.GetAacProfile();
			uint8_t aac_sample_rate = (uint8_t)aac_specific_config.SamplingFrequency();
			uint8_t aac_channels = (uint8_t)aac_specific_config.Channel();

			auto adts_header = MakeHeader(aac_profile, aac_sample_rate, aac_channels, aac_raw_length);

			annexb_data->Append(adts_header);
		}
		
		annexb_data->Append(data);
	}

	data->Clear();

	if(annexb_data->GetLength() > 0)
	{
		data->Append(annexb_data);
	}

	return true;
}

std::shared_ptr<ov::Data> AACLatmToAdts::MakeHeader(uint8_t aac_profile, uint8_t aac_sample_rate, uint8_t aac_channels, int16_t data_length)
{
	uint8_t ADTS_HEADER_LENGTH = 7;
	int16_t aac_frame_length = data_length + 7;

	ov::BitWriter bits(ADTS_HEADER_LENGTH);

	bits.Write(12, 0x0FFF);				// syncword [12b]
	bits.Write(1, 0);					// ID - 0=MPEG-4, 1=MPEG-2 [1b]
	bits.Write(2, 0);					// layer - Always 0 [2b]
	bits.Write(1, 1);					// protection_absent  [1b]
	bits.Write(2, aac_profile);			// profile [2b]
	bits.Write(4, aac_sample_rate);		// sampling_frequency_index[[4b]
	bits.Write(1, 0);					// private_bit[1b]
	bits.Write(3, aac_channels);		// channel_configuration[3b]
	bits.Write(1, 0);					// Original/copy[1b]
	bits.Write(1, 0);					// Home[1b]
	bits.Write(1, 0);					// copyright_identification_bit[1b]
	bits.Write(1, 0);					// copyright_identification_start[1b]
	bits.Write(13, aac_frame_length);	// aac_frame_length[13b]
	bits.Write(11, 0x3F);				// adts_buffer_fullness[11b]
	bits.Write(2, 0);					// no_raw_data_blocks_inframe[2b]

	std::shared_ptr<ov::Data> data = std::make_shared<ov::Data>(bits.GetData(), bits.GetDataSize());

	return data;
}