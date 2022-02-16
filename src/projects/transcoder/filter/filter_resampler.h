//==============================================================================
//
//  Transcode
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include "../transcoder_context.h"
#include "base/mediarouter/media_buffer.h"
#include "base/mediarouter/media_type.h"
#include "filter_base.h"

class MediaFilterResampler : public MediaFilterImpl
{
public:
	MediaFilterResampler();
	~MediaFilterResampler();

	bool Configure(const std::shared_ptr<MediaTrack> &input_media_track, const std::shared_ptr<TranscodeContext> &input_context, const std::shared_ptr<TranscodeContext> &output_context) override;

	int32_t SendBuffer(std::shared_ptr<MediaFrame> buffer) override;
	std::shared_ptr<MediaFrame> RecvBuffer(TranscodeResult *result) override;

	void FilterThread();

	bool Start() override;
	void Stop();
};
