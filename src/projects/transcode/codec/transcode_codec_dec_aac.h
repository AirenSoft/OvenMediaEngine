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

class OvenCodecImplAvcodecDecAAC : public OvenCodecImpl
{
public:
    OvenCodecImplAvcodecDecAAC(); 
    ~OvenCodecImplAvcodecDecAAC();

public:
 	/*virtual*/
 	int32_t Configure(std::shared_ptr<TranscodeContext> context) override;

	void sendbuf(std::unique_ptr<MediaBuffer> buf) override;
	
	std::pair<int32_t, std::unique_ptr<MediaBuffer>> recvbuf() override;
	
private:
	std::shared_ptr<TranscodeContext> _transcode_context;

	AVCodecContext*					_context;
	AVCodecParserContext*			_parser;
	AVCodecParameters *				_codec_par;
	std::vector<std::unique_ptr<MediaBuffer>> 	_pkt_buf;

	bool							_change_format;

	AVPacket*						_pkt;
	AVFrame*						_frame;
	int 							_decoded_frame_num;
};
