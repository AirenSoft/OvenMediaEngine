//==============================================================================
//
//  TranscoderStreamInternal
//
//  Created by Keukhan
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/info/application.h>

#include <stdint.h>
#include <memory>
#include <queue>
#include <vector>

#include "base/info/stream.h"
#include "base/mediarouter/media_type.h"
#include "transcoder_context.h"

class TranscoderStreamInternal
{
public:
	TranscoderStreamInternal();
	~TranscoderStreamInternal();

public:
	MediaTrackId NewTrackId();
	std::atomic<MediaTrackId> _last_track_index = 0;

	static ov::String ProfileToSerialize(const uint32_t track_id, const cfg::vhost::app::oprf::VideoProfile &profile);
	static ov::String ProfileToSerialize(const uint32_t track_id, const cfg::vhost::app::oprf::AudioProfile &profile);
	static ov::String ProfileToSerialize(const uint32_t track_id, const cfg::vhost::app::oprf::ImageProfile &profile);
	static ov::String ProfileToSerialize(const uint32_t track_id);
	
	static cmn::Timebase GetDefaultTimebaseByCodecId(cmn::MediaCodecId codec_id);

	std::shared_ptr<MediaTrack> CreateOutputTrack(const std::shared_ptr<MediaTrack> &input_track, const cfg::vhost::app::oprf::VideoProfile &profile);
	std::shared_ptr<MediaTrack> CreateOutputTrack(const std::shared_ptr<MediaTrack> &input_track, const cfg::vhost::app::oprf::AudioProfile &profile);
	std::shared_ptr<MediaTrack> CreateOutputTrack(const std::shared_ptr<MediaTrack> &input_track, const cfg::vhost::app::oprf::ImageProfile &profile);
	std::shared_ptr<MediaTrack> CreateOutputTrackDataType(const std::shared_ptr<MediaTrack> &input_track);	

	bool IsMatchesBypassCondition(const std::shared_ptr<MediaTrack> &input_track, const cfg::vhost::app::oprf::VideoProfile &profile);
	bool IsMatchesBypassCondition(const std::shared_ptr<MediaTrack> &input_track, const cfg::vhost::app::oprf::AudioProfile &profile);

	double GetProperFramerate(const std::shared_ptr<MediaTrack> &ref_track);
	static double MeasurementToRecommendFramerate(double framerate);

	void UpdateOutputTrackPassthrough(const std::shared_ptr<MediaTrack> &output_track, MediaFrame *buffer);
	void UpdateOutputTrackTranscode(const std::shared_ptr<MediaTrack> &output_track, const std::shared_ptr<MediaTrack> &input_track, MediaFrame *buffer);


public:
	// For the purpose of checking if the input track has changed during the update.
	bool StoreInputTrackSnapshot(std::shared_ptr<info::Stream> stream);
	std::map<int32_t, std::shared_ptr<MediaTrack>>& GetInputTrackSnapshot();
	bool IsEqualCountAndMediaTypeOfMediaTracks(std::map<int32_t, std::shared_ptr<MediaTrack>> a, std::map<int32_t, std::shared_ptr<MediaTrack>> b);

private:
	std::map<int32_t, std::shared_ptr<MediaTrack>> _input_track_snapshot;	
};
