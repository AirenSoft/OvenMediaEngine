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
#include "base/provider/application.h"
#include "base/provider/stream.h"

#include "media_router/bitstream/bitstream_to_annexb.h"
#include "media_router/bitstream/bitstream_to_adts.h"

using namespace pvd;

class RtmpStream : public pvd::Stream
{
public:
	static std::shared_ptr<RtmpStream> Create(const std::shared_ptr<pvd::Application> &application, const uint32_t stream_id, const ov::String &stream_name);

public:
	explicit RtmpStream(const std::shared_ptr<pvd::Application> &application, const info::Stream &stream_info);
	~RtmpStream() final;

	bool Start() override;
	bool Stop() override;

	bool ConvertToVideoData(const std::shared_ptr<ov::Data> &data, int64_t &cts);
	uint32_t ConvertToAudioData(const std::shared_ptr<ov::Data> &data);

private:
	// bitstream filters
	BitstreamToAnnexB _bsfv;
	BitstreamToADTS _bsfa;
};