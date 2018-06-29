//==============================================================================
//
//  Transcode
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include <vector>
#include <stdint.h>

#include "transcode_codec_impl.h"

class OvenCodecImplAvcodecEncAAC : public OvenCodecImpl
{
public:
    OvenCodecImplAvcodecEncAAC(); 
    ~OvenCodecImplAvcodecEncAAC();

public:
 	/*virtual*/
 	int Configure(std::shared_ptr<TranscodeContext> context);
 	void sendbuf(std::unique_ptr<MediaBuffer> buf);
 	std::pair<int, std::unique_ptr<MediaBuffer>> recvbuf();

private:
	std::shared_ptr<TranscodeContext> _transcode_context;

	AVCodecContext*					_context;
	std::vector<std::unique_ptr<MediaBuffer>> 	_pkt_buf;

	AVPacket*						_pkt;
	AVFrame*						_frame;
	int 							_decoded_frame_num;
};
