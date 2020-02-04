//==============================================================================
//
//  RtmpProvider
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include "base/common_types.h"
#include "base/provider/stream.h"

#include "media_router/bitstream/bitstream_to_annexb.h"
#include "media_router/bitstream/bitstream_to_adts.h"

using namespace pvd;

class RtmpStream : public pvd::Stream
{
public:
	static std::shared_ptr<RtmpStream> Create(const std::shared_ptr<pvd::Application> &application);

public:
	explicit RtmpStream(const std::shared_ptr<pvd::Application> &application);
	~RtmpStream() final;

	bool ConvertToVideoData(const std::shared_ptr<ov::Data> &data, int64_t &cts);
	bool ConvertToAudioData(const std::shared_ptr<ov::Data> &data);
private:

	
	// bitstream filters
	BitstreamToAnnexB _bsfv;
	BitstreamToADTS _bsfa;
};