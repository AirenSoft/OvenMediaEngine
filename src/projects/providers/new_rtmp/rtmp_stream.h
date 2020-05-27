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
#include "base/provider/push_provider/push_provider_stream.h"

#include "media_router/bitstream/bitstream_to_annexb.h"
#include "media_router/bitstream/bitstream_to_adts.h"

using namespace pvd;

class RtmpStream : public pvd::PushProviderStream
{
public:
	static std::shared_ptr<RtmpStream> Create(const std::shared_ptr<PushProvider> &provider);

public:
	explicit RtmpStream(const std::shared_ptr<PushProvider> &provider);
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