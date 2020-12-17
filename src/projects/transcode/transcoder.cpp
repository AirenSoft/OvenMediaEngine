#include <utility>

//==============================================================================
//
//  Transcoder
//
//  Created by Kwon Keuk Han
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#include <unistd.h>

#include <iostream>

#include "config/config_manager.h"
#include "transcode_private.h"
#include "transcoder.h"

std::shared_ptr<Transcoder> Transcoder::Create(std::shared_ptr<MediaRouteInterface> router)
{
	auto transcoder = std::make_shared<Transcoder>(router);
	if (!transcoder->Start())
	{
		logte("An error occurred while creating Transcoder");
		return nullptr;
	}
	return transcoder;
}

Transcoder::Transcoder(std::shared_ptr<MediaRouteInterface> router)
{
	_router = std::move(router);
}

bool Transcoder::Start()
{
	logti("Transcoder has been started.");
	return true;
}

bool Transcoder::Stop()
{
	logti("Transcoder has been stopped.");
	return true;
}

// Create Application
bool Transcoder::OnCreateApplication(const info::Application &app_info)
{
	auto application_id = app_info.GetId();

	auto application = TranscodeApplication::Create(app_info);

	_tracode_apps[application_id] = application;

	// Register to MediaRouter
	if (_router->RegisterObserverApp(app_info, application) == false)
	{
		logte("Could not register the application: %p", application.get());

		return false;
	}

	// Register to MediaRouter
	if (_router->RegisterConnectorApp(app_info, application) == false)
	{
		logte("Could not register the application: %p", application.get());

		return false;
	}

	logti("Transcoder has created [%s][%s] application", app_info.IsDynamicApp() ? "dynamic" : "config", app_info.GetName().CStr());

	return true;
}

// Delete Application
bool Transcoder::OnDeleteApplication(const info::Application &app_info)
{
	auto application_id = app_info.GetId();
	auto it = _tracode_apps.find(application_id);
	if (it == _tracode_apps.end())
	{
		return false;
	}

	auto application = it->second;
	application->Stop();

	// Unregister to MediaRouter
	if (_router->UnregisterObserverApp(app_info, application) == false)
	{
		logte("Could not unregister the application: %p", application.get());
	}

	// Unregister to MediaRouter
	if (_router->UnregisterConnectorApp(app_info, application) == false)
	{
		logte("Could not unregister the application: %p", application.get());
	}

	_tracode_apps.erase(it);

	logti("Transcoder has deleted [%s][%s] application", app_info.IsDynamicApp() ? "dynamic" : "config", app_info.GetName().CStr());

	return true;
}

//  Application Name으로 TranscodeApplication 찾음
std::shared_ptr<TranscodeApplication> Transcoder::GetApplicationById(info::application_id_t application_id)
{
	auto obj = _tracode_apps.find(application_id);

	if (obj == _tracode_apps.end())
	{
		return nullptr;
	}

	return obj->second;
}
