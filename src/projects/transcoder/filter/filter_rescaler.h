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

class FilterRescaler : public FilterBase
{
public:
	FilterRescaler();
	~FilterRescaler();

	bool Configure(const std::shared_ptr<MediaTrack> &input_media_track, const std::shared_ptr<TranscodeContext> &input_context, const std::shared_ptr<TranscodeContext> &output_context) override;

	int32_t SendBuffer(std::shared_ptr<MediaFrame> buffer) override;

	void FilterThread();

	bool Start() override;
	void Stop() override;

protected:
};
