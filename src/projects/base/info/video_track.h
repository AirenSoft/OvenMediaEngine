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

	// Return the proper framerate for this track. 
	// If there is a framerate set by the user, it is returned. If not, the automatically measured framerate is returned	
	double GetFrameRate() const;

	// Estimated at intervals between frames
	void SetEstimateFrameRate(double framerate);
	double GetEstimateFrameRate() const;

	void SetFrameRateByMeasured(double framerate);
	double GetFrameRateByMeasured() const;

	void SetFrameRateLastSecond(double framerate);
	double GetFrameRateLastSecond() const;

	void SetFrameRateByConfig(double framerate);
	double GetFrameRateByConfig() const;

	void SetWidth(int32_t width);
	int32_t GetWidth() const;

	void SetWidthByConfig(int32_t width);
	int32_t GetWidthByConfig() const;

	void SetHeight(int32_t height);
	int32_t GetHeight() const;

	void SetHeightByConfig(int32_t height);
	int32_t GetHeightByConfig() const;

	void SetVideoTimestampScale(double scale);
	double GetVideoTimestampScale() const;

	void SetHasBframes(bool has_bframe);
	bool HasBframes() const;

	void SetColorspace(int colorspace);
	int GetColorspace() const;	

	void SetPreset(ov::String preset);
	ov::String GetPreset() const;

	void SetProfile(ov::String profile);
	ov::String GetProfile() const;

	void SetThreadCount(int thread_count);
	int GetThreadCount();

	// Return the proper key_frame_interval for this track. 
	// If there is a key_frame_interval set by the user, it is returned. If not, the automatically measured key_frame_interval is returned
	int32_t GetKeyFrameInterval() const;

	void SetKeyFrameIntervalByMeasured(double key_frame_interval);
	double GetKeyFrameIntervalByMeasured() const;

	void SetKeyFrameIntervalLastet(int32_t key_frame_interval);
	int32_t GetKeyFrameIntervalLatest() const;
	
	void SetKeyFrameIntervalByConfig(int32_t key_frame_interval);
	int32_t GetKeyFrameIntervalByConfig() const;

	void SetKeyFrameIntervalTypeByConfig(cmn::KeyFrameIntervalType key_frame_interval_type);
	cmn::KeyFrameIntervalType GetKeyFrameIntervalTypeByConfig() const;

	void SetDeltaFrameCountSinceLastKeyFrame(int32_t delta_frame_count);
	int32_t GetDeltaFramesSinceLastKeyFrame() const;

	void SetBFrames(int32_t b_frames);
	int32_t GetBFrames();

	void SetSkipFramesByConfig(int32_t skip_frames);
	int32_t GetSkipFramesByConfig() const;

protected:

	// framerate (measurement)
	double _framerate = 0;
	// framerate (set by user)
	double _framerate_conf = 0;
	// framerate (estimated) 
	double _framerate_estimated = 0;
	// framerate last one second (measurement)
	double _framerate_last_second = 0;

	double _video_timescale;
	
	// Resolution
	int32_t _width;
	int32_t _height;

	// Resolution (set by user)
	int32_t _width_conf;
	int32_t _height_conf;

	// Key Frame Interval Avg (measurement)
	double _key_frame_interval = 0;
	// Key Frame Interval Latest (measurement)
	int32_t _key_frame_interval_latest = 0;
	// Key Frame Interval (set by user)
	int32_t _key_frame_interval_conf = 0;
	// Delta Frame Count since last key frame
	int32_t _delta_frame_count_since_last_key_frame = 0;

	// Key Frame Interval Type (set by user)
	cmn::KeyFrameIntervalType _key_frame_interval_type_conf;

	// Number of B-frame (set by user)
	int32_t _b_frames = 0;
	
	// B-frame (set by mediarouter)
	bool _has_bframe = false;

	// Colorspace of video
	// This variable is temporarily used in the Pixel Format defined by FFMPEG.
	int _colorspace;	

	// Preset for encoder (set by user)
	ov::String _preset;

	// Profile (set by user, used for h264, h265 codec)
	ov::String _profile;
	
	// Thread count of codec (set by user)
	int _thread_count;	

	// Skip frames (set by user)
	// If the set value is greater than or equal to 0, the skip frame is automatically calculated. 
	// The skip frame is not less than the value set by the user.
	// -1 : No SkipFrame
	// 0 ~ 120 : minimum value of SkipFrames. it is automatically calculated and the SkipFrames value is changed.
	int32_t _skip_frames_conf = -1;
};
