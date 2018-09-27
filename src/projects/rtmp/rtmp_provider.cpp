//==============================================================================
//
//  RtmpProvider
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include <iostream>
#include <unistd.h>

#include "rtmp_provider.h"
#include "rtmp_application.h"
#include "rtmp_stream.h"

#define OV_LOG_TAG "RtmpProvider"

using namespace MediaCommonType;
//====================================================================================================
// Create
//====================================================================================================
std::shared_ptr<RtmpProvider> RtmpProvider::Create(std::shared_ptr<MediaRouteInterface> router)
{
	auto provider = std::make_shared<RtmpProvider>(router);
	provider->Start();
	return provider;
}
//====================================================================================================
// RtmpProvider
//====================================================================================================
RtmpProvider::RtmpProvider(std::shared_ptr<MediaRouteInterface> router) 
	: Provider(router)
{
	logtd("Created Rtmp Provider modules.");
}
//====================================================================================================
// ~RtmpProvider
//====================================================================================================
RtmpProvider::~RtmpProvider()
{
	Stop();
	logtd("Terminated Rtmp Provider modules.");
}

//====================================================================================================
// Start
//====================================================================================================
bool RtmpProvider::Start()
{
    logtd("RtmpProvider Start");

	// RtmpServer 생성
	_rtmp_server = std::make_shared<RtmpServer>();

	// RtmpServer에 Observer 연결
	_rtmp_server->AddObserver(RtmpObserver::GetSharedPtr());
	_rtmp_server->Start(ov::SocketAddress(1935));

	return Provider::Start();
}

//====================================================================================================
// Stop
//====================================================================================================
bool RtmpProvider::Stop()
{
	return Provider::Stop();
}	

std::shared_ptr<Application> RtmpProvider::OnCreateApplication(ApplicationInfo &info)
{
	return RtmpApplication::Create(info);
}

//====================================================================================================
// OnStreamReadyComplete
// - RtmpObserver 구현
//====================================================================================================
bool RtmpProvider::OnStreamReadyComplete(const ov::String &app_name, const ov::String &stream_name, std::shared_ptr<RtmpMediaInfo> &media_info, uint32_t &app_id, uint32_t &stream_id)
{
    logtd("OnStreamReadyComplete - app(%s) stream(%s)", app_name.CStr(), stream_name.CStr());

    // 어플리케이션 조회, 어플리케이션명에 해당하는 정보가 없다면 RTMP 커넥션을 종료함.
    auto application = std::dynamic_pointer_cast<RtmpApplication>(GetApplication(app_name.CStr()));
    if(application == nullptr)
    {
        return false;
    }

    // Application -> RtmpApplication: create rtmp stream -> Application 에 Stream 정보 저장
    auto stream = application->MakeStream();
    if(stream == nullptr)
    {
        logte("can not create stream");
        return false;
    }

    stream->SetName(stream_name.CStr());

    if(media_info->has_video)
    {
        auto new_track = std::make_shared<MediaTrack>();

        // 비디오는 TrackId 0으로 고정
        new_track->SetId(0);
        new_track->SetMediaType(MediaType::Video);
        new_track->SetCodecId(MediaCodecId::H264);
        new_track->SetWidth((uint32_t)media_info->video_width);
        new_track->SetHeight((uint32_t)media_info->video_height);
        new_track->SetFrameRate(media_info->video_framerate);

        // RTMP는 1/1000 단위의 타임 베이스를 사용함
        new_track->SetTimeBase(1, 1000);
        stream->AddTrack(new_track);
    }

    if(media_info->has_aduio)
    {
        auto new_track = std::make_shared<MediaTrack>();

        // 오디오는 TrackID를 1로 고정
        new_track->SetId(1);
        new_track->SetMediaType(MediaType::Audio);
        new_track->SetCodecId(MediaCodecId::Aac);
        new_track->SetSampleRate(media_info->audio_samplerate);
        new_track->SetTimeBase(1, 1000);
        new_track->GetSample().SetFormat(MediaCommonType::AudioSample::Format::S16);

        // new_track->SetSampleSize(conn->_audio_samplesize);

        if      (media_info->audio_channels == 1)          new_track->GetChannel().SetLayout(MediaCommonType::AudioChannel::Layout::LayoutMono);
        else if(media_info->audio_channels == 2)    new_track->GetChannel().SetLayout(MediaCommonType::AudioChannel::Layout::LayoutStereo);

        new_track->GetChannel().SetLayout(MediaCommonType::AudioChannel::Layout::LayoutStereo);

        stream->AddTrack(new_track);
    }

    // 라우터에 스트림이 생성되었다고 알림
    application->CreateStream2(stream);

    // id 설정
   app_id = application->GetId();
   stream_id = stream->GetId();


	return true;
}
//====================================================================================================
// OnVideoStreamData
// - RtmpObserver 구현
//====================================================================================================
bool RtmpProvider::OnVideoData(uint32_t app_id, uint32_t stream_id, uint32_t timestamp, std::shared_ptr<std::vector<uint8_t>> &data)
{
    auto application = std::dynamic_pointer_cast<RtmpApplication>(GetApplication((int32_t)app_id));

    if(application == nullptr)
    {
        logte("can not find application");
        return false;
    }

    auto stream = std::dynamic_pointer_cast<RtmpStream>(GetStream((uint32_t)app_id, (uint32_t)stream_id));
    if(stream == nullptr)
    {
        logte("can not find stream");
        return false;
    }

    auto pbuf = std::make_unique<MediaPacket>(MediaType::Video, 0, data->data(), data->size(), timestamp, MediaPacketFlag::NoFlag);

    application->SendFrame(stream, std::move(pbuf));

    return true;
}

//====================================================================================================
// OnAudioStreamData
// - RtmpObserver 구현
//====================================================================================================
bool RtmpProvider::OnAudioData(uint32_t app_id, uint32_t stream_id, uint32_t timestamp, std::shared_ptr<std::vector<uint8_t>> &data)
{
    auto application = std::dynamic_pointer_cast<RtmpApplication>(GetApplication((int32_t)app_id));

    if(application == nullptr)
    {
        logte("can not find application");
        return false;
    }

    auto stream = std::dynamic_pointer_cast<RtmpStream>(GetStream((uint32_t)app_id, (uint32_t)stream_id));

    if(stream == nullptr)
    {
        logte("can not find stream");
        return false;
    }

    auto pbuf = std::make_unique<MediaPacket>(MediaType::Audio, 1,data->data(), data->size(), timestamp, MediaPacketFlag::NoFlag);
    application->SendFrame(stream, std::move(pbuf));

    return true;
}

//====================================================================================================
// OnDeleteStream
// - RtmpObserver 구현
//====================================================================================================
bool RtmpProvider::OnDeleteStream(uint32_t app_id, uint32_t stream_id)
{
    auto application = std::dynamic_pointer_cast<RtmpApplication>(GetApplication((int32_t)app_id));
    if(application == nullptr)
    {
        logte("can not find application");
        return false;
    }

    auto stream = std::dynamic_pointer_cast<RtmpStream>(GetStream((uint32_t)app_id, (uint32_t)stream_id));
    if(stream == nullptr)
    {
        logte("can not find stream");
        return false;
    }

	// 라우터에 스트림이 삭제되었다고 알림
	return application->DeleteStream2(stream);
}
