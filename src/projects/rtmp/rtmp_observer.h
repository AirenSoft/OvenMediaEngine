//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <memory>
#include <base/ovlibrary/ovlibrary.h>

class RtmpObserver : public ov::EnableSharedFromThis<RtmpObserver>
{
public:
	// 스트림 정보 수신 완료
	virtual bool OnStreamReadyComplete(const ov::String &app_name, const ov::String &stream_name, std::shared_ptr<RtmpMediaInfo> &media_info, uint32_t &app_id, uint32_t &stream_id) = 0;

	// video 스트림 데이트 수신
	virtual bool OnVideoData(uint32_t app_id, uint32_t stream_id, uint32_t timestamp, std::shared_ptr<std::vector<uint8_t>> &data) = 0;

	// audio 스트림 데이터 수신
	virtual bool OnAudioData(uint32_t app_id, uint32_t stream_id, uint32_t timestamp, std::shared_ptr<std::vector<uint8_t>> &data) = 0;

    // 스트림 삭제
    virtual bool OnDeleteStream(uint32_t app_id, uint32_t stream_id) = 0;

};
