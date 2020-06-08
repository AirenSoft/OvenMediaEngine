//==============================================================================
//
//  Bit Stream Converter
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include <base/ovlibrary/ovlibrary.h>

#include "bitstream_to_adts.h"
#include "bitstream_to_annexa.h"
#include "bitstream_to_annexb.h"

class BitstreamConv
{
public:
	// Definition of Conversion Type
	typedef enum _ConvType {
		ConvUnknown = 0,
		ConvADTS,
		ConvAnnexA,
		ConvAnnexB
	} ConvType;

	// Result Type
	typedef enum _ResultType {
		Failed = 0,
		SuccessWithData,
		SuccessWithoutData
	} ResultType;

public:
    BitstreamConv();
    ~BitstreamConv();

    void setType(ConvType type);
    
 	ResultType Convert(ConvType conv_type, const std::shared_ptr<ov::Data> &data);
 	ResultType Convert(ConvType conv_type, const std::shared_ptr<ov::Data> &data, int64_t &cts);

private:
	ConvType 			_type;
	BitstreamToADTS 	_adts;
	BitstreamToAnnexA 	_annexa;
	BitstreamToAnnexB 	_annexb;
};
