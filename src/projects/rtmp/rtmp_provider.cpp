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

std::shared_ptr<RtmpProvider> RtmpProvider::Create(std::shared_ptr<MediaRouteInterface> router)
{
	auto provider = std::make_shared<RtmpProvider>(router);
	provider->Start();
	return provider;
}

RtmpProvider::RtmpProvider(std::shared_ptr<MediaRouteInterface> router) 
	: Provider(router)
{
	logtd("Created Rtmp Provider modules.");
}

RtmpProvider::~RtmpProvider()
{
	Stop();
	logtd("Terminated Rtmp Provider modules.");
}

bool RtmpProvider::Start()
{
	RtmpTcpServer::Start();
	
	return Provider::Start();
}

bool RtmpProvider::Stop()
{
	RtmpTcpServer::Stop();

	return Provider::Stop();
}	

std::shared_ptr<Application> RtmpProvider::OnCreateApplication(ApplicationInfo &info)
{
	return RtmpApplication::Create(info);
}

//============================================================================================================
// Rtmp Tcp Server 인터페이스
//============================================================================================================

int RtmpProvider::OnConnect(RtmpConnector *conn, ov::String app, ov::String flashVer, ov::String swfUrl, ov::String tcUrl) 
{
	logtd("onConnect rtmp(%p), app(%s), flashVer(%s), swfUrl(%s), tcUrl(%s)", conn, app.CStr(), flashVer.CStr(), swfUrl.CStr(), tcUrl.CStr());

	// 어플리케이션 조회, 어플리케이션명에 해당하는 정보가 없다면 RTMP 커넥션을 종료함.
	auto application = std::dynamic_pointer_cast<RtmpApplication>(GetApplication(app.CStr()));
	if(application == nullptr)
	{
		return -1;
	}

	// 커덱션에 어플리케이션 아이디 정보를 저장함
	conn->_app_id = application->GetId();

	return 0;
}

void RtmpProvider::OnReleaseStream(RtmpConnector *conn, ov::String stream) 
{
	logtd("onReleaseStream rtmp(%p), stream(%s / %s)", conn, conn->GetApplication().c_str(), conn->GetStream().c_str());
}


void RtmpProvider::OnFCPublish(RtmpConnector *conn, ov::String stream) 
{
	logtd("onFCPublish rtmp(%p), stream(%s / %s)", conn, conn->GetApplication().c_str(), conn->GetStream().c_str());
}


void RtmpProvider::OnCreateStream(RtmpConnector *conn) 
{
	logd("RtmpProvider", "onCreateStream rtmp(%p), stream(%s / %s)", conn, conn->GetApplication().c_str(), conn->GetStream().c_str());

	// 스트림 생성
}

void RtmpProvider::OnPublish(RtmpConnector *conn, ov::String stream, ov::String type) 
{
	logd("RtmpProvider", "onPublish rtmp(%p), stream(%s / %s)", conn, conn->GetApplication().c_str(), conn->GetStream().c_str());
}


void RtmpProvider::OnMetaData(RtmpConnector *conn) 
{
	logd("RtmpProvider", "onMetaData rtmp(%p), stream(%s / %s)", conn, conn->GetApplication().c_str(), conn->GetStream().c_str());

	// 스트림 기본 정보를 가져옴
	auto application = std::dynamic_pointer_cast<RtmpApplication>(GetApplication((int64_t)conn->_app_id));
	if(application == nullptr)
	{
		logte("can not find application"); 
		return;
	}

	// Application -> RtmpApplication: create rtmp stream -> Application 에 Stream 정보 저장
	auto stream = application->MakeStream();
	if(stream == nullptr)
	{
		logte("can not create stream");
		return;
	}

	stream->SetName(conn->GetStream().c_str());

	if(conn->_has_video == true) 
	{
		auto new_track = std::make_shared<MediaTrack>();
	
		// 비디오는 TrackId 0으로 고정
		new_track->SetId(0);
		new_track->SetMediaType(MediaType::MEDIA_TYPE_VIDEO);
		new_track->SetCodecId(MediaCodecId::CODEC_ID_H264);
		new_track->SetWidth((uint32_t)conn->_width);
		new_track->SetHeight((uint32_t)conn->_height);
		new_track->SetFrameRate(conn->_framerate);
		
		// RTMP는 1/1000 단위의 타임 베이스를 사용함
		new_track->SetTimeBase(1, 1000);	
		stream->AddTrack(new_track);
	} 


	if(conn->_has_audio == true)
	{
		auto new_track = std::make_shared<MediaTrack>();
	
		// 오디오는 TrackID를 1로 고정
		new_track->SetId(1);
		new_track->SetMediaType(MediaType::MEDIA_TYPE_AUDIO);
		new_track->SetCodecId(MediaCodecId::CODEC_ID_AAC);
		new_track->SetSampleRate(conn->_audio_samplerate);
		new_track->SetTimeBase(1, 1000);
		new_track->GetSample().SetFormat(MediaCommonType::AudioSample::Format::SAMPLE_FMT_S16);

		// new_track->SetSampleSize(conn->_audio_samplesize);

		if((int32_t)conn->_audio_channels == 1)
			new_track->GetChannel().SetLayout(MediaCommonType::AudioChannel::Layout::AUDIO_LAYOUT_MONO);
		else if((int32_t)conn->_audio_channels == 2)
			new_track->GetChannel().SetLayout(MediaCommonType::AudioChannel::Layout::AUDIO_LAYOUT_STEREO);
		
		new_track->GetChannel().SetLayout(MediaCommonType::AudioChannel::Layout::AUDIO_LAYOUT_STEREO);
		
		stream->AddTrack(new_track);		
	}

	// 라우터에 스트림이 생성되었다고 알림
	application->CreateStream2(stream);

	// RTMP 커넥션에 스트림 정보를 저장함
	conn->_stream_id = stream->GetId();
}


// 멈춤
void RtmpProvider::OnFCUnpublish(RtmpConnector *conn, ov::String stream) 
{
	logd("RtmpProvider", "onFCUnpublish rtmp(%p), stream(%s / %s)", conn, conn->GetApplication().c_str(), conn->GetStream().c_str());
}


void RtmpProvider::OnDeleteStream(RtmpConnector *conn) 
{
	logd("RtmpProvider", "onDeleteStream rtmp(%p), stream(%s / %s)", conn, conn->GetApplication().c_str(), conn->GetStream().c_str());

	auto application = std::dynamic_pointer_cast<RtmpApplication>(GetApplication((int64_t)conn->_app_id));
	if(application == nullptr)
	{
		logte("can not find application"); 
		return;
	}

	auto stream = std::dynamic_pointer_cast<RtmpStream>(GetStream((uint32_t)conn->_app_id, (uint32_t)conn->_stream_id));
	if(stream == nullptr)
	{
		logte("can not find stream"); 
		return;
	}

	// 라우터에 스트림이 삭제되었다고 알림
	application->DeleteStream2(stream);
}


void RtmpProvider::OnVideoPacket(RtmpConnector *conn, int8_t has_abs_time, uint32_t time, int8_t* body, uint32_t body_size) 
{
	// logd("RtmpProvider", "OnVideoPacket. abs(%d) time %u size %u bytes", has_abs_time, time, body_size);

	// logd("RtmpProvider", "Stage-1 : %u", time);

	auto application = std::dynamic_pointer_cast<RtmpApplication>(GetApplication((int64_t)conn->_app_id));
	if(application == nullptr)
	{
		logte("can not find application"); 
		return;
	}

	auto stream = std::dynamic_pointer_cast<RtmpStream>(GetStream((uint32_t)conn->_app_id, (uint32_t)conn->_stream_id));
	if(stream == nullptr)
	{
		logte("can not find stream"); 
		return;
	}

	auto pbuf = std::make_unique<MediaBuffer>(MediaType::MEDIA_TYPE_VIDEO, 0, (uint8_t*)body, body_size, time);

	application->SendFrame(stream, std::move(pbuf));
}


void RtmpProvider::OnAudioPacket(RtmpConnector *conn, int8_t has_abs_time, uint32_t time, int8_t* body, uint32_t body_size) 
{
	// logd("RtmpProvider", "OnAudioPacket. rtmp(%p), abs(%d) time %u size %u bytes", conn, has_abs_time, time, body_size);
	
	auto application = std::dynamic_pointer_cast<RtmpApplication>(GetApplication((int64_t)conn->_app_id));
	if(application == nullptr)
	{
		logte("can not find application"); 
		return;
	}

	auto stream = std::dynamic_pointer_cast<RtmpStream>(GetStream((uint32_t)conn->_app_id, (uint32_t)conn->_stream_id));
	if(stream == nullptr)
	{
		logte("can not find stream"); 
		return;
	}

	auto pbuf = std::make_unique<MediaBuffer>(MediaType::MEDIA_TYPE_AUDIO, 1, (uint8_t*)body, body_size, time);
	
	application->SendFrame(stream, std::move(pbuf));
}


void RtmpProvider::OnDisconnect(RtmpConnector *conn) 
{
	logd("RtmpProvider", "OnDisconnect rtmp(%p), stream(%s / %s)", conn, conn->GetApplication().c_str(), conn->GetStream().c_str());
}
