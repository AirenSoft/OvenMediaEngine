//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "base/common_types.h"
#include <modules/bitstream/nalu/nal_unit_fragment_header.h>

class VideoTrack
{
public:
	VideoTrack();

	void SetFrameRate(double framerate);
	double GetFrameRate() const;

	void SetEstimateFrameRate(double framerate);
	double GetEsimateFrameRate() const;

	void SetWidth(int32_t width);
	int32_t GetWidth() const;

	void SetHeight(int32_t height);
	int32_t GetHeight() const;

	void SetFormat(int32_t format);
	int32_t GetFormat() const;

	void SetVideoTimestampScale(double scale);
	double GetVideoTimestampScale() const;

	// Codec-specific data prepared in advance for performance
	std::shared_ptr<ov::Data> GetH264SpsPpsAnnexBFormat() const;
	const FragmentationHeader& GetH264SpsPpsAnnexBFragmentHeader() const;
	void SetH264SpsPpsAnnexBFormat(const std::shared_ptr<ov::Data>& data, const FragmentationHeader &header);

protected:
	double _framerate;
	double _estimate_framerate;
	double _video_timescale;
	int32_t _width;
	int32_t _height;
	int32_t _format;

	std::shared_ptr<ov::Data> _h264_sps_pps_annexb_data = nullptr;
	FragmentationHeader _h264_sps_pps_annexb_fragment_header;
};
