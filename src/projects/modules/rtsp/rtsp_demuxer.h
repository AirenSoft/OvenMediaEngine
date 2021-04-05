//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2021 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/ovlibrary/ovlibrary.h>
#include "rtsp_message.h"
#include "rtsp_data.h"

class RtspDemuxer
{
public:
	RtspDemuxer();
	bool AppendPacket(const std::shared_ptr<ov::Data> &packet);

	bool IsAvailableMessage();
	bool IsAvaliableData();

	std::shared_ptr<RtspMessage> PopMessage();
	std::shared_ptr<ov::Data> PopData();

private:
	std::shared_ptr<ov::Data> _buffer;

	ov::Queue<std::shared_ptr<RtspMessage>>	_messages;
	// Interleaved binary data
	ov::Queue<std::shared_ptr<RtspData>> _datas;
};