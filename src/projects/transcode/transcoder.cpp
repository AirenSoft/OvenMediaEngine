//==============================================================================
//
//  Transcoder
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#include <iostream>
#include <unistd.h>

#include "transcoder.h"
#include "config/config_manager.h"

#define OV_LOG_TAG "Transcoder"

std::shared_ptr<Transcoder> Transcoder::Create(std::shared_ptr<MediaRouteInterface> router)
{
	auto media_router = std::make_shared<Transcoder>(router);
	media_router->Start();
	return media_router;
}

Transcoder::Transcoder(std::shared_ptr<MediaRouteInterface> router)
{
	_router = router;
}

Transcoder::~Transcoder()
{
}

bool Transcoder::Start()
{
	logtd("Started media trancode modules.");

	// 데이터베이스(or Config)에서 어플리케이션 정보를 생성함.
	if(!CraeteApplications())
	{
		logte("Failed to start media transcode modules. invalid application.");

		return false;
	}

	return true;
}

bool Transcoder::Stop()
{
	logtd("Terminated media transcode modules.");

	if(!DeleteApplication())
	{
		return false;
	}

	// TODO: 패킷 처리 스레드를 만들어야함.. 어플리케이션 단위로 만들어 버릴까?
	return true;
}

// 어플리케이션의 스트림이 생성됨
// TODO: Global Config에서 설정값을 읽어옴
bool Transcoder::CraeteApplications()
{
	auto application_infos = ConfigManager::Instance()->GetApplicationInfos();
	for(auto const &application_info : application_infos)
	{
		//TODO(soulk) : 어플리케이션 구분 코드를 Name에서 Id 체계로 변경해야함.
		ov::String key_app = application_info->GetName();

		auto trans_app = std::make_shared<TranscodeApplication>(application_info);

		// // 라우터 어플리케이션 관리 항목에 추가
		_tracode_apps.insert(
			std::make_pair(key_app.CStr(), trans_app)
		);

		_router->RegisterObserverApp(application_info, trans_app);
		_router->RegisterConnectorApp(application_info, trans_app);
	}

	return true;
}

// 어플리케이션의 스트림이 삭제됨
bool Transcoder::DeleteApplication()
{
	return true;
}

//  Application Name으로 TranscodeApplication 찾음
std::shared_ptr<TranscodeApplication> Transcoder::GetApplicationByName(std::string app_name)
{
	auto obj = _tracode_apps.find(app_name);
	if(obj == _tracode_apps.end())
	{
		return NULL;
	}

	return obj->second;
}
