//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2021 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/ovlibrary/ovlibrary.h>

class ChunkedTransferInterface
{
public:
	virtual ~ChunkedTransferInterface() = default;

	// This callback will be called when each frame is received
	virtual void OnCmafChunkDataPush(const ov::String &app_name, const ov::String &stream_name,
									 const ov::String &file_name,
									 bool is_video,
									 std::shared_ptr<ov::Data> &chunk_data) = 0;

	virtual void OnCmafChunkedComplete(const ov::String &app_name, const ov::String &stream_name,
									   const ov::String &file_name,
									   bool is_video) = 0;
};