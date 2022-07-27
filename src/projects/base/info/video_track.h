//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <modules/bitstream/h264/h264_parser.h>
#include <modules/bitstream/nalu/nal_unit_fragment_header.h>

#include "base/common_types.h"

class VideoTrack
{
public:
	VideoTrack();

	void SetFrameRate(double framerate);
	double GetFrameRate() const;

	void SetEstimateFrameRate(double framerate);
	double GetEstimateFrameRate() const;

	void SetWidth(int32_t width);
	int32_t GetWidth() const;

	void SetHeight(int32_t height);
	int32_t GetHeight() const;

	void SetVideoTimestampScale(double scale);
	double GetVideoTimestampScale() const;

	// Codec-specific data prepared in advance for performance
	std::shared_ptr<ov::Data> GetH264SpsPpsAnnexBFormat() const;
	const FragmentationHeader& GetH264SpsPpsAnnexBFragmentHeader() const;
	void SetH264SpsPpsAnnexBFormat(const std::shared_ptr<ov::Data>& data, const FragmentationHeader& header);
	void SetH264SpsData(const std::shared_ptr<ov::Data>& data);
	void SetH264PpsData(const std::shared_ptr<ov::Data>& data);
	std::shared_ptr<ov::Data> GetH264SpsData() const;
	std::shared_ptr<ov::Data> GetH264PpsData() const;

	//@Set by Configuration
	void SetPreset(ov::String preset);
	ov::String GetPreset() const;

	//@Set by mediarouter
	void SetHasBframes(bool has_bframe);
	bool HasBframes();

	// @Set By Configuration
	void SetThreadCount(int thread_count);
	int GetThreadCount();

	//@Set by Configuration
	void SetKeyFrameInterval(int32_t key_frame_interval);
	int32_t GetKeyFrameInterval();

	//@Set by Configuration
	void SetBFrames(int32_t b_frames);
	int32_t GetBFrames();

protected:
	double _framerate;
	double _estimate_framerate;
	double _video_timescale;
	int32_t _width;
	int32_t _height;
	int32_t _key_frame_interval;
	int32_t _b_frames;
	bool _has_bframe;

	ov::String _preset;
	std::shared_ptr<ov::Data> _h264_sps_pps_annexb_data = nullptr;
	std::shared_ptr<ov::Data> _h264_sps_data = nullptr;
	std::shared_ptr<ov::Data> _h264_pps_data = nullptr;

	FragmentationHeader _h264_sps_pps_annexb_fragment_header;
	H264SPS _h264_sps;
	

	int _thread_count;

public:
	void SetColorspace(int colorspace);
	int GetColorspace() const;	
	// Colorspace
	// - This variable is temporarily used in the Pixel Format defined by FFMPEG.
	int _colorspace;	
};
