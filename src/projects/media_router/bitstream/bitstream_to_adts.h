//==============================================================================
//
//  MediaRouter
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include "base/media_route/media_buffer.h"
#include "base/media_route/media_type.h"
#include <stdint.h>

#define AudioCodecIdDisabled 17
#define AudioCodecIdMP3      2
#define AudioCodecIdAAC      10

class BitstreamToADTS
{
	enum CodecAudioType
	{
	    CodecAudioTypeReserved					= 2,
	    CodecAudioTypeSequenceHeader			= 0,
	    CodecAudioTypeRawData					= 1,
	};

	enum AacObjectType
	{
	    AacObjectTypeReserved = 0,
	    
	    // Table 1.1 - Audio Object Type definition
	    // @see @see aac-mp4a-format-ISO_IEC_14496-3+2001.pdf, page 23
	    AacObjectTypeAacMain = 1,
	    AacObjectTypeAacLC = 2,
	    AacObjectTypeAacSSR = 3,
	    
	    // AAC HE = LC+SBR
	    AacObjectTypeAacHE = 5,
	    // AAC HEv2 = LC+SBR+PS
	    AacObjectTypeAacHEV2 = 29,
	};

	enum AacProfile
	{
	    AacProfileReserved = 3,
	    
	    // @see 7.1 Profiles, aac-iso-13818-7.pdf, page 40
	    AacProfileMain = 0,
	    AacProfileLC = 1,
	    AacProfileSSR = 2,
	};

public:
    BitstreamToADTS();
    ~BitstreamToADTS();

 	void convert_to(const std::shared_ptr<ov::Data> &data);

 	static bool SequenceHeaderParsing(const uint8_t *data,
									  int data_size,
									  int &sample_index,
									  int &channels);
private:
	AacProfile 	codec_aac_rtmp2ts(AacObjectType object_type);

	bool						_has_sequence_header;
	
	AacObjectType 				aac_object;
	int8_t 						aac_sample_rate;
	int8_t 						aac_channels;
};

