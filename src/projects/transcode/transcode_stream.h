//==============================================================================
//
//  TranscodeStream
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once


#include <stdint.h>
#include <memory>
#include <vector>
#include <queue>

#include "base/media_route/media_buffer.h"
#include "base/media_route/media_queue.h"
#include "base/media_route/media_type.h"
#include "base/application/stream_info.h"

#include "transcode_codec.h"
#include "transcode_context.h"
#include "transcode_filter.h"

class TranscodeApplication;
class TranscodeStream
{
public:
    TranscodeStream(std::shared_ptr<StreamInfo> orig_stream_info, TranscodeApplication* parent);
    ~TranscodeStream();

    void Stop();

    bool Push(std::unique_ptr<MediaBuffer> frame);
    uint32_t GetBufferCount();

    std::shared_ptr<StreamInfo> GetStreamInfo();

private:

    // 입력 스트림 정보
    std::shared_ptr<StreamInfo> _stream_info_input;

    // 출력(변화된) 스트림 정보
    std::shared_ptr<StreamInfo> _stream_info_output;
    
    // 미디어 인코딩된 원본 패킷 버퍼
    MediaQueue<std::unique_ptr<MediaBuffer>> _queue;

    // 디코딩된 프레임 버퍼
    MediaQueue<std::unique_ptr<MediaBuffer>> _queue_decoded;

    // 필터링된 프레임 버퍼
    MediaQueue<std::unique_ptr<MediaBuffer>> _queue_filterd;


private:
    // 트랜스코딩 코덱 변환 정보
    std::shared_ptr<TranscodeContext> _transcode_context;

    // 디코더
    // KEY=MediaTrackId
    std::map<int32_t, std::unique_ptr<TranscodeCodec>> _decoders;

    // 인코더
    // Key=MediaTrackId
    std::map<int32_t, std::unique_ptr<TranscodeCodec>> _encoders;

    // 필터
    // Key=MediaTrackId
    std::map<int32_t, std::unique_ptr<TranscodeFilter>> _filters;


private:
    volatile bool _kill_flag;

    void DecodeTask();
    std::thread _thread_decode;

    void FilterTask();
    std::thread _thread_filter;

    void EncodeTask();
    std::thread _thread_encode;

    TranscodeApplication* _parent;

    // 디코더 생성
    void CreateDecoder(int32_t track_id);

    // 인코더 생성
    void CreateEncoder(std::shared_ptr<MediaTrack> media_track, std::shared_ptr<TranscodeContext> transcode_context);
    
    // 디코딩된 프레임의 포맷이 분석되거나 변경될 경우 호출됨.
    void ChangeOutputFormat(MediaBuffer* buffer);
    
    // 1. 디코딩
    int32_t do_decode(int32_t track_id, std::unique_ptr<MediaBuffer> frame);
    // 2. 필터링
    int32_t do_filter(int32_t track_id, std::unique_ptr<MediaBuffer> frame);
    // 3. 인코딩
    int32_t do_encode(int32_t track_id, std::unique_ptr<MediaBuffer> frame);


    // 통계 정보
private:
    uint32_t _stats_decoded_frame_count;
};

