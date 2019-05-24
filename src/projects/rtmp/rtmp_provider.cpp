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
#include <config/config.h>

#include "rtmp_provider.h"
#include "rtmp_application.h"
#include "rtmp_stream.h"

#define OV_LOG_TAG "RtmpProvider"

using namespace common;

//====================================================================================================
// Create
//====================================================================================================
std::shared_ptr<RtmpProvider>
RtmpProvider::Create(const info::Application *application_info, std::shared_ptr<MediaRouteInterface> router)
{
	auto provider = std::make_shared<RtmpProvider>(application_info, router);
	provider->Start();
	return provider;
}

//====================================================================================================
// RtmpProvider
//====================================================================================================
RtmpProvider::RtmpProvider(const info::Application *application_info, std::shared_ptr<MediaRouteInterface> router)
	: Provider(application_info, router)
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
	// Find RTMP provider configuration
	_provider_info = _application_info->GetProvider<cfg::RtmpProvider>();

	if(_provider_info == nullptr)
	{
		logte("Cannot initialize RtmpProvider using config information");
		return false;
	}

	auto host = _application_info->GetParentAs<cfg::Host>("Host");

	if(host == nullptr)
	{
		OV_ASSERT2(false);
		return false;
	}

	int port = host->GetPorts().GetRtmpProviderPort().GetPort();

	// auto rtmp_provider = host->
	auto rtmp_address = ov::SocketAddress(host->GetIp(), static_cast<uint16_t>(port));

	logti("RTMP Provider is listening on %s...", rtmp_address.ToString().CStr());

	// RtmpServer 생성
	_rtmp_server = std::make_shared<RtmpServer>();

	// RtmpServer 에 Observer 연결
	_rtmp_server->AddObserver(RtmpObserver::GetSharedPtr());
	_rtmp_server->Start(rtmp_address);

	return Provider::Start();
}

//====================================================================================================
// Stop
//====================================================================================================
bool RtmpProvider::Stop()
{
	return Provider::Stop();
}

std::shared_ptr<Application> RtmpProvider::OnCreateApplication(const info::Application *application_info)
{
	return RtmpApplication::Create(application_info);
}

//====================================================================================================
// OnStreamReadyComplete
// - RtmpObserver 구현
//====================================================================================================
bool RtmpProvider::OnStreamReadyComplete(const ov::String &app_name,
                                         const ov::String &stream_name,
                                         std::shared_ptr<RtmpMediaInfo> &media_info,
                                         info::application_id_t &application_id,
                                         uint32_t &stream_id)
{
	// 어플리케이션 조회, 어플리케이션명에 해당하는 정보가 없다면 RTMP 커넥션을 종료함.
	auto application = std::dynamic_pointer_cast<RtmpApplication>(GetApplicationByName(app_name.CStr()));
	if(application == nullptr)
	{
		logte("Cannot Find Applicaton - app(%s) stream(%s)", app_name.CStr(), stream_name.CStr());
		return false;
	}

	if(GetStreamByName(app_name, stream_name))
	{
		if(_provider_info->IsBlockDuplicateStreamName())
		{
			logti("Duplicate Stream Input(reject) - app(%s) stream(%s)", app_name.CStr(), stream_name.CStr());
			return false;
		}
		else
		{
			uint32_t stream_id = GetStreamByName(app_name, stream_name)->GetId();

			logti("Duplicate Stream Input(change) - app(%s) stream(%s)", app_name.CStr(), stream_name.CStr());

			// session close
			if(!_rtmp_server->Disconnect(app_name, stream_id))
			{
				logte("Disconnect Fail - app(%s) stream(%s)", app_name.CStr(), stream_name.CStr());
			}

			// 스트림 정보 종료
			application->DeleteStream2(application->GetStreamByName(stream_name));

		}
	}

	// Application -> RtmpApplication: create rtmp stream -> Application 에 Stream 정보 저장
	auto stream = application->MakeStream();
	if(stream == nullptr)
	{
		logte("can not create stream - app(%s) stream(%s)", app_name.CStr(), stream_name.CStr());
		return false;
	}

	stream->SetName(stream_name.CStr());

	if(media_info->video_streaming)
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

	if(media_info->audio_streaming)
	{
		auto new_track = std::make_shared<MediaTrack>();

		// 오디오는 TrackID를 1로 고정
		new_track->SetId(1);
		new_track->SetMediaType(MediaType::Audio);
		new_track->SetCodecId(MediaCodecId::Aac);
		new_track->SetSampleRate(media_info->audio_samplerate);
		new_track->SetTimeBase(1, 1000);
		new_track->GetSample().SetFormat(common::AudioSample::Format::S16);

		// new_track->SetSampleSize(conn->_audio_samplesize);

		if(media_info->audio_channels == 1)
		{
			new_track->GetChannel().SetLayout(common::AudioChannel::Layout::LayoutMono);
		}
		else if(media_info->audio_channels == 2)
		{
			new_track->GetChannel().SetLayout(common::AudioChannel::Layout::LayoutStereo);
		}

		stream->AddTrack(new_track);
	}

	// 라우터에 스트림이 생성되었다고 알림
	application->CreateStream2(stream);

	// id 설정
	application_id = application->GetId();
	stream_id = stream->GetId();

	logtd("Strem ready complete - app(%s/%u) stream(%s/%u)", app_name.CStr(), application_id, stream_name.CStr(), stream_id);

	return true;
}

//====================================================================================================
// OnVideoStreamData
// - RtmpObserver 구현
//====================================================================================================
bool RtmpProvider::OnVideoData(info::application_id_t application_id,
                               uint32_t stream_id,
                               uint32_t timestamp,
                               RtmpFrameType frame_type,
                               std::shared_ptr<std::vector<uint8_t>> &data)
{
	auto application = std::dynamic_pointer_cast<RtmpApplication>(GetApplicationById(application_id));

	if(application == nullptr)
	{
		logte("cannot find application");
		return false;
	}

	auto stream = std::dynamic_pointer_cast<RtmpStream>(application->GetStreamById(stream_id));
	if(stream == nullptr)
	{
		logte("cannot find stream");
		return false;
	}

	auto pbuf = std::make_unique<MediaPacket>(MediaType::Video,
	                                          0,
	                                          data->data(),
	                                          data->size(),
	                                          timestamp,
	                                          frame_type == RtmpFrameType::VideoIFrame ? MediaPacketFlag::Key : MediaPacketFlag::NoFlag);

	application->SendFrame(stream, std::move(pbuf));

	return true;
}

//====================================================================================================
// OnAudioStreamData
// - RtmpObserver 구현
//====================================================================================================
bool RtmpProvider::OnAudioData(info::application_id_t application_id,
                               uint32_t stream_id,
                               uint32_t timestamp,
                               RtmpFrameType frame_type,
                               std::shared_ptr<std::vector<uint8_t>> &data)
{
	auto application = std::dynamic_pointer_cast<RtmpApplication>(GetApplicationById(application_id));

	if(application == nullptr)
	{
		logte("cannot find application");
		return false;
	}

	auto stream = std::dynamic_pointer_cast<RtmpStream>(application->GetStreamById(stream_id));

	if(stream == nullptr)
	{
		logte("cannot find stream");
		return false;
	}

	auto pbuf = std::make_unique<MediaPacket>(MediaType::Audio,
	                                          1,
	                                          data->data(),
	                                          data->size(),
	                                          timestamp,
	                                          MediaPacketFlag::Key);
	application->SendFrame(stream, std::move(pbuf));

	return true;
}

//====================================================================================================
// OnDeleteStream
// - RtmpObserver 구현
//====================================================================================================
bool RtmpProvider::OnDeleteStream(info::application_id_t app_id, uint32_t stream_id)
{
	auto application = std::dynamic_pointer_cast<RtmpApplication>(GetApplicationById(app_id));
	if(application == nullptr)
	{
		logte("cannot find application");
		return false;
	}

	auto stream = std::dynamic_pointer_cast<RtmpStream>(application->GetStreamById(stream_id));
	if(stream == nullptr)
	{
		logte("cannot find stream");
		return false;
	}

	// 라우터에 스트림이 삭제되었다고 알림
	return application->DeleteStream2(stream);
}
