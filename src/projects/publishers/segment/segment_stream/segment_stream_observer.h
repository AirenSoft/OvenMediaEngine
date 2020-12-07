//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include <base/ovlibrary/ovlibrary.h>

#include <memory>

#include "segment_stream_request_info.h"
#include "stream_packetizer.h"

//====================================================================================================
// SegmentStreamObserver
//====================================================================================================
class SegmentStreamObserver : public ov::EnableSharedFromThis<SegmentStreamObserver>
{
public:
	// Called when the client requests a playlist (such as .m3u8, .mpd)
	virtual bool OnPlayListRequest(const std::shared_ptr<HttpClient> &client,
								   const SegmentStreamRequestInfo &request_info,
								   ov::String &play_list) = 0;

	// Called when the client requests a segment (such as .ts, .m4s)
	virtual bool OnSegmentRequest(const std::shared_ptr<HttpClient> &client,
								  const SegmentStreamRequestInfo &request_info,
								  std::shared_ptr<const SegmentItem> &segment) = 0;
};
