//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "chunk/rtmp_define.h"

#include <memory>
#include <base/ovlibrary/ovlibrary.h>
#include <config/config.h>

class RtmpObserver : public ov::EnableSharedFromThis<RtmpObserver>
{
public:
	// 스트림 정보 수신 완료
	virtual bool OnStreamReadyComplete(const ov::String &app_name, const ov::String &stream_name, std::shared_ptr<RtmpMediaInfo> &media_info, info::application_id_t &application_id, uint32_t &stream_id) = 0;

	// video 스트림 데이트 수신
	virtual bool OnVideoData(info::application_id_t application_id, uint32_t stream_id, uint32_t timestamp, RtmpFrameType frame_type, std::shared_ptr<std::vector<uint8_t>> &data) = 0;

	// audio 스트림 데이터 수신
	virtual bool OnAudioData(info::application_id_t application_id, uint32_t stream_id, uint32_t timestamp, RtmpFrameType frame_type, std::shared_ptr<std::vector<uint8_t>> &data) = 0;

	// 스트림 삭제
	virtual bool OnDeleteStream(info::application_id_t application_id, uint32_t stream_id) = 0;

};
