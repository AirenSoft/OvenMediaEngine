//==============================================================================
//
//  Transcode
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include <stdint.h>
#include <vector>
#include <stdint.h>
#include <memory>
#include <vector>
#include <algorithm>
#include <thread>

extern "C" {
	#include <libavcodec/avcodec.h>
	#include <libavutil/pixdesc.h>
	#include <libavutil/opt.h>
}

#include "base/media_route/media_buffer.h"
#include "base/media_route/media_type.h"
#include "../transcode_context.h"

class OvenCodecImpl
{
public:
	OvenCodecImpl() {};
    virtual ~OvenCodecImpl() {};

    virtual int32_t Configure(std::shared_ptr<TranscodeContext> context) = 0;
 
    virtual void sendbuf(std::unique_ptr<MediaBuffer> buf) = 0;

	// 리턴값 정의
	//   0 : 프레임 디코딩 완료, 아직 디코딩할 데이터가 대기하고 있음.
	//   1 : 포맷이 변경이 되었다.
	//   < 0 : 뭐든간에 에러가 있다.
    virtual std::pair<int32_t, std::unique_ptr<MediaBuffer>> recvbuf() = 0;

public:
	int64_t	_last_send_packet_pts;
	int64_t	_last_recv_packet_pts;
};

