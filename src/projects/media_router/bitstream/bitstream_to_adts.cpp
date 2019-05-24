//==============================================================================
//
//  MediaRouter
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "bitstream_to_adts.h"

#include <cstdio>

#include <base/ovlibrary/ovlibrary.h>

#define OV_LOG_TAG "BitstreamToADTS"

BitstreamToADTS::BitstreamToADTS()
{
	_has_sequence_header = false;
}

BitstreamToADTS::~BitstreamToADTS()
{
}

BitstreamToADTS::AacProfile BitstreamToADTS::codec_aac_rtmp2ts(AacObjectType object_type)
{
	switch(object_type)
	{
		case AacObjectTypeAacMain:
			return AacProfileMain;
		case AacObjectTypeAacHE:
		case AacObjectTypeAacHEV2:
		case AacObjectTypeAacLC:
			return AacProfileLC;
		case AacObjectTypeAacSSR:
			return AacProfileSSR;
		default:
			return AacProfileReserved;
	}
}

void BitstreamToADTS::convert_to(const std::shared_ptr<ov::Data> &data)
{
	if(data->GetLength() == 0)
	{
		return;
	}

	uint8_t *pbuf = data->GetWritableDataAs<uint8_t>();

	// 만약, ADTS 타입이면  그냥 종료함.
	if(pbuf[0] == 0xff)
	{
		logtd("already ADTS type");
		return;
	}

	uint8_t sound_format = pbuf[0];

	uint8_t sound_type = sound_format & 0x01;
	uint8_t sound_size = (sound_format >> 1) & 0x01;
	uint8_t sound_rate = (sound_format >> 2) & 0x03;
	uint8_t audio_codec_id = (sound_format >> 4) & 0x0f;

	uint8_t aac_packet_type = pbuf[1];

#if 1
	logtd("sound_type:%d, sound_size:%d, sound_rate:%d, audio_codec_id:%d, aac_packet_type:%d",
	      sound_type, sound_size, sound_rate, audio_codec_id, aac_packet_type);
#endif

	if(audio_codec_id != AudioCodecIdAAC)
	{
		logtw("aac reqired. format=%d", audio_codec_id);
	}

	// 시퀀스 헤더
	if(aac_packet_type == CodecAudioTypeSequenceHeader)
	{
		uint8_t audioObjectType = pbuf[2];
		aac_sample_rate = pbuf[3];

		aac_channels = (aac_sample_rate >> 3) & 0x0f;
		aac_sample_rate = ((audioObjectType << 1) & 0x0e) | ((aac_sample_rate >> 7) & 0x01);

		audioObjectType = (audioObjectType >> 3) & 0x1f;
		aac_object = (AacObjectType)audioObjectType;

		logtd("audio object type = %d, aac_sample_rate = %d, aac_channels = %d", audioObjectType, aac_sample_rate, aac_channels);

		_has_sequence_header = true;

		logtd("detected aac sequence header\r");

		data->Clear();

		return;
	}
	else if(aac_packet_type == CodecAudioTypeRawData)
	{
		if(_has_sequence_header == false)
		{
			return;
		}

		int16_t aac_fixed_header_length = 2;

		// extract data after [aac_fixed_header_length] bytes
		// packet->EraseBuffer(0, aac_fixed_header_length);
		data->Erase(0, aac_fixed_header_length);
		// pkt->_buf.erase(pkt->_buf.begin(), pkt->_buf.begin()+aac_fixed_header_length);

		int16_t aac_raw_length = data->GetLength();    // 2바이트 헤더를 제외함

		////////////////////////////////////////////////////////////
		// ADTS 헤더를 추가함
		////////////////////////////////////////////////////////////
		char aac_fixed_header[7];
		if(true)
		{
			char *pp = aac_fixed_header;
			int16_t aac_frame_length = aac_raw_length + 7;

			// Syncword 12 bslbf
			*pp++ = 0xff;
			// 4bits left.
			// adts_fixed_header(), 1.A.2.2.1 Fixed Header of ADTS
			// ID 1 bslbf
			// Layer 2 uimsbf
			// protection_absent 1 bslbf
			*pp++ = 0xf1;

			// profile 2 uimsbf
			// sampling_frequency_index 4 uimsbf
			// private_bit 1 bslbf
			// channel_configuration 3 uimsbf
			// original/copy 1 bslbf
			// home 1 bslbf
			AacProfile aac_profile = codec_aac_rtmp2ts(aac_object);
			*pp++ = ((aac_profile << 6) & 0xc0) | ((aac_sample_rate << 2) & 0x3c) | ((aac_channels >> 2) & 0x01);
			// 4bits left.
			// adts_variable_header(), 1.A.2.2.2 Variable Header of ADTS
			// copyright_identification_bit 1 bslbf
			// copyright_identification_start 1 bslbf
			*pp++ = ((aac_channels << 6) & 0xc0) | ((aac_frame_length >> 11) & 0x03);

			// aac_frame_length 13 bslbf: Length of the frame including headers and error_check in bytes.
			// use the left 2bits as the 13 and 12 bit,
			// the aac_frame_length is 13bits, so we move 13-2=11.
			*pp++ = aac_frame_length >> 3;
			// adts_buffer_fullness 11 bslbf
			*pp++ = (aac_frame_length << 5) & 0xe0;

			// no_raw_data_blocks_in_frame 2 uimsbf
			*pp++ = 0xfc;
		}

		data->Insert(aac_fixed_header, 0, sizeof(aac_fixed_header));
	}
}

//====================================================================================================
// BitstreamSequenceInfoParsing
// - Bitstream(Rtmp Input Low Data) Sequence Info Parsing
//====================================================================================================
bool BitstreamToADTS::SequenceHeaderParsing(const uint8_t *data,
											 int data_size,
											 int &sample_index,
											 int &channels)
{
	if(data_size < 4)
	{
		return false;
	}

	if(((data[0] >> 4) & 0x0f) != AudioCodecIdAAC || data[1] != CodecAudioTypeSequenceHeader)
	{
		return false;
	}

	channels = (data[3] >> 3) & 0x0f;
	sample_index = ((data[2] << 1) & 0x0e) | ((data[3] >> 7) & 0x01);
	//aac_type = (AacObjectType)((data[2] >> 3) & 0x1f);

	return true;
}
