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
#include "stream_packetizer.h"

//====================================================================================================
// SegmentStreamObserver
//====================================================================================================
class SegmentStreamObserver : public ov::EnableSharedFromThis<SegmentStreamObserver>
{
public:
	// PlayList 요청
	virtual bool OnPlayListRequest(const ov::String &app_name, const ov::String &stream_name,
								   const ov::String &file_name,
								   ov::String &play_list) = 0;

	// Segment 요청
	virtual bool OnSegmentRequest(const ov::String &app_name, const ov::String &stream_name,
								  const ov::String &file_name,
								  std::shared_ptr<SegmentData> &segment) = 0;
};
