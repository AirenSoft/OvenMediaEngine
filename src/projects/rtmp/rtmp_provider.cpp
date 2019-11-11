//==============================================================================
//
//  RtmpProvider
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include <config/config.h>
#include <unistd.h>
#include <iostream>

#include "rtmp_application.h"
#include "rtmp_provider.h"
#include "rtmp_stream.h"

#define OV_LOG_TAG "RtmpProvider"

using namespace common;

std::shared_ptr<RtmpProvider> RtmpProvider::Create(const info::Host &host_info, const std::shared_ptr<MediaRouteInterface> &router)
{
	auto provider = std::make_shared<RtmpProvider>(host_info, router);
	if (!provider->Start())
	{
		logte("An error occurred while creating RtmpProvider");
		return nullptr;
	}
	return provider;
}

RtmpProvider::RtmpProvider(const info::Host &host_info, const std::shared_ptr<MediaRouteInterface> &router)
	: Provider(host_info, router)
{
	logtd("Created Rtmp Provider module.");
}

RtmpProvider::~RtmpProvider()
{
	Stop();
	logtd("Terminated Rtmp Provider modules.");
}

bool RtmpProvider::Start()
{
	// Get Host configuration
	auto host = GetHostInfo();

	// auto rtmp_provider = host->
	auto rtmp_address = ov::SocketAddress(host.GetIp(), static_cast<uint16_t>(host.GetPorts().GetRtmpProviderPort().GetPort()));

	logti("RTMP Provider is listening on %s...", rtmp_address.ToString().CStr());

	// RtmpServer 생성
	_rtmp_server = std::make_shared<RtmpServer>();

	// RtmpServer 에 Observer 연결
	_rtmp_server->AddObserver(RtmpObserver::GetSharedPtr());

	if (!_rtmp_server->Start(rtmp_address))
	{
		return false;
	}

	return Provider::Start();
}

bool RtmpProvider::Stop()
{
	return Provider::Stop();
}

std::shared_ptr<Application> RtmpProvider::OnCreateProviderApplication(const info::Application &application_info)
{
	return RtmpApplication::Create(application_info);
}

bool RtmpProvider::OnStreamReadyComplete(const ov::String &app_name,
										 const ov::String &stream_name,
										 std::shared_ptr<RtmpMediaInfo> &media_info,
										 info::application_id_t &application_id,
										 uint32_t &stream_id)
{
	// 어플리케이션 조회, 어플리케이션명에 해당하는 정보가 없다면 RTMP 커넥션을 종료함.
	auto application = std::dynamic_pointer_cast<RtmpApplication>(GetApplicationByName(app_name.CStr()));
	if (application == nullptr)
	{
		logte("Cannot Find Applicaton - app(%s) stream(%s)", app_name.CStr(), stream_name.CStr());
		return false;
	}

	auto provider_info = application->GetProvider<cfg::RtmpProvider>();
	if(provider_info == nullptr)
	{
		logte("Cannot Find ProviderInfo from Applicaton - app(%s) stream(%s)", app_name.CStr(), stream_name.CStr());
		return false;
	}

	// Handle duplicated stream name
	if (GetStreamByName(app_name, stream_name))
	{
		if (provider_info->IsBlockDuplicateStreamName())
		{
			logti("Duplicate Stream Input(reject) - app(%s) stream(%s)", app_name.CStr(), stream_name.CStr());
			return false;
		}
		else
		{
			uint32_t stream_id = GetStreamByName(app_name, stream_name)->GetId();

			logti("Duplicate Stream Input(change) - app(%s) stream(%s)", app_name.CStr(), stream_name.CStr());

			// session close
			if (!_rtmp_server->Disconnect(app_name, stream_id))
			{
				logte("Disconnect Fail - app(%s) stream(%s)", app_name.CStr(), stream_name.CStr());
			}

			// 스트림 정보 종료
			application->NotifyStreamDeleted(application->GetStreamByName(stream_name));
		}
	}

	// Application -> RtmpApplication: create rtmp stream -> Application 에 Stream 정보 저장
	auto stream = application->CreateProviderStream();
	if (stream == nullptr)
	{
		logte("can not create stream - app(%s) stream(%s)", app_name.CStr(), stream_name.CStr());
		return false;
	}

	stream->SetName(stream_name.CStr());

	if (media_info->video_streaming)
	{
		auto new_track = std::make_shared<MediaTrack>();

		// 비디오는 TrackId 0으로 고정
		new_track->SetId(0);
		new_track->SetMediaType(MediaType::Video);
		new_track->SetCodecId(MediaCodecId::H264);
		new_track->SetWidth((uint32_t)media_info->video_width);
		new_track->SetHeight((uint32_t)media_info->video_height);
		new_track->SetFrameRate(media_info->video_framerate);

		// I know RTMP uses 1/1000 timebase, however, this timebase was used due to low precision.
		// new_track->SetTimeBase(1, 1000);
		new_track->SetTimeBase(1, 90000);

		// A value to change to 1/90000 from 1/1000
		_video_scale = 90000.0 / 1000.0;

		stream->AddTrack(new_track);
	}

	if (media_info->audio_streaming)
	{
		auto new_track = std::make_shared<MediaTrack>();

		// 오디오는 TrackID를 1로 고정
		new_track->SetId(1);
		new_track->SetMediaType(MediaType::Audio);
		new_track->SetCodecId(MediaCodecId::Aac);
		new_track->SetSampleRate(media_info->audio_samplerate);
		new_track->GetSample().SetFormat(common::AudioSample::Format::S16);

		// new_track->SetSampleSize(conn->_audio_samplesize);

		if (media_info->audio_channels == 1)
		{
			new_track->GetChannel().SetLayout(common::AudioChannel::Layout::LayoutMono);
		}
		else if (media_info->audio_channels == 2)
		{
			new_track->GetChannel().SetLayout(common::AudioChannel::Layout::LayoutStereo);
		}

		// I know RTMP uses 1/1000 timebase, however, this timebase was used due to low precision.
		// new_track->SetTimeBase(1, 1000);
		new_track->SetTimeBase(1, media_info->audio_samplerate);

		// A value to change to 1/sample_rate from 1/1000
		_audio_scale = (double)(media_info->audio_samplerate) / 1000.0;

		stream->AddTrack(new_track);
	}

	// 라우터에 스트림이 생성되었다고 알림
	application->NotifyStreamCreated(stream);

	// id 설정
	application_id = application->GetId();
	stream_id = stream->GetId();

	logtd("Strem ready complete - app(%s/%u) stream(%s/%u)", app_name.CStr(), application_id, stream_name.CStr(), stream_id);

	return true;
}

bool RtmpProvider::OnVideoData(info::application_id_t application_id,
							   uint32_t stream_id,
							   int64_t timestamp,
							   RtmpFrameType frame_type,
							   const std::shared_ptr<const ov::Data> &data)
{
	auto application = std::dynamic_pointer_cast<RtmpApplication>(GetApplicationById(application_id));
	if (application == nullptr)
	{
		logte("cannot find application");
		return false;
	}

	auto stream = std::dynamic_pointer_cast<RtmpStream>(application->GetStreamById(stream_id));
	if (stream == nullptr)
	{
		logte("cannot find stream");
		return false;
	}

	auto pbuf = std::make_unique<MediaPacket>(MediaType::Video,
											  0,
											  data,
											  // The timestamp used by RTMP is DTS. PTS will be recalculated later
											  timestamp * _video_scale,  // PTS
											  timestamp * _video_scale,  // DTS
											  // RTMP doesn't know frame's duration
											  -1LL,
											  frame_type == RtmpFrameType::VideoIFrame ? MediaPacketFlag::Key : MediaPacketFlag::NoFlag);

	application->SendFrame(stream, std::move(pbuf));

	return true;
}

bool RtmpProvider::OnAudioData(info::application_id_t application_id,
							   uint32_t stream_id,
							   int64_t timestamp,
							   RtmpFrameType frame_type,
							   const std::shared_ptr<const ov::Data> &data)
{
	auto application = std::dynamic_pointer_cast<RtmpApplication>(GetApplicationById(application_id));

	if (application == nullptr)
	{
		logte("cannot find application");
		return false;
	}

	auto stream = std::dynamic_pointer_cast<RtmpStream>(application->GetStreamById(stream_id));

	if (stream == nullptr)
	{
		logte("cannot find stream");
		return false;
	}

	auto pbuf = std::make_unique<MediaPacket>(MediaType::Audio,
											  1,
											  data,
											  // The timestamp used by RTMP is DTS. PTS will be recalculated later
											  timestamp * _audio_scale,  // PTS
											  timestamp * _audio_scale,  // DTS
											  // RTMP doesn't know frame's duration
											  -1LL,
											  MediaPacketFlag::Key);
	application->SendFrame(stream, std::move(pbuf));

	return true;
}

bool RtmpProvider::OnDeleteStream(info::application_id_t app_id, uint32_t stream_id)
{
	auto application = std::dynamic_pointer_cast<RtmpApplication>(GetApplicationById(app_id));
	if (application == nullptr)
	{
		logte("cannot find application");
		return false;
	}

	auto stream = std::dynamic_pointer_cast<RtmpStream>(application->GetStreamById(stream_id));
	if (stream == nullptr)
	{
		logte("cannot find stream");
		return false;
	}

	// 라우터에 스트림이 삭제되었다고 알림
	return application->NotifyStreamDeleted(stream);
}
