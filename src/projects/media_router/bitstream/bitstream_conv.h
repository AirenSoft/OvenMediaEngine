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
		INVALID_DATA = 0,
		SUCCESS_DATA,
		SUCCESS_NODATA
	} ResultType;

public:
	BitstreamConv();
	~BitstreamConv();

	void SetType(ConvType type);

	ResultType Convert(const std::shared_ptr<ov::Data> &data);
	ResultType Convert(const std::shared_ptr<ov::Data> &data, int64_t &cts);

	ResultType Convert(ConvType type, const std::shared_ptr<ov::Data> &data);
	ResultType Convert(ConvType type, const std::shared_ptr<ov::Data> &data, int64_t &cts);

 	static bool ParseSequenceHeaderAVC(const uint8_t *data,
		int data_size,
		std::vector<uint8_t> &_sps,
		std::vector<uint8_t> &_pps,
		uint8_t &avc_profile,
		uint8_t &avc_profile_compatibility,
		uint8_t &avc_level);

 	static bool MakeAVCFragmentHeader(const std::shared_ptr<MediaPacket> &packet);

private:
	ConvType 			_type;
	BitstreamToADTS 	_adts;
	BitstreamToAnnexA 	_annexa;
	BitstreamToAnnexB 	_annexb;
};
