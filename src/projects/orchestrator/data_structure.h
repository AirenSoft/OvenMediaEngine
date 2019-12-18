//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/info/info.h>
#include <base/ovlibrary/ovlibrary.h>

enum class OrchestratorModuleType
{
	Unknown,
	Provider,
	MediaRouter,
	Transcoder,
	Publisher
};

const char *GetOrchestratorModuleTypeName(OrchestratorModuleType type);

class OrchestratorModuleInterface : public ov::EnableSharedFromThis<OrchestratorModuleInterface>
{
public:
	virtual OrchestratorModuleType GetModuleType() const = 0;

	//--------------------------------------------------------------------
	// Event callbacks
	//--------------------------------------------------------------------
	/// Called when the application is created
	///
	/// @param app_info The information of the application
	virtual bool OnCreateApplication(const info::Application &app_info) = 0;

	/// Called when the application is deleted
	///
	/// @param app_info The information of the application
	virtual bool OnDeleteApplication(const info::Application &app_info) = 0;
};

class OrchestratorProviderModuleInterface : public OrchestratorModuleInterface
{
public:
	OrchestratorModuleType GetModuleType() const override
	{
		return OrchestratorModuleType::Provider;
	}

	/// Called when trying to determine whether an app can be created from urls
	///
	/// @param url_list The URLs to determine
	///
	/// @return Returns true if the URLs is valid and the stream can be obtained
	virtual bool CheckOriginsAvailability(const std::vector<ov::String> &url_list) = 0;

	/// Called when another module is requested to pull stream list
	///
	/// @param url_list The streaming URL list to pull
	///
	/// @return Returns true if successfully pulled and finishes creating the stream, false otherwise
	virtual bool PullStreams(info::application_id_t app_id, const ov::String &app_name, const ov::String stream_name, const std::vector<ov::String> &url_list) = 0;
};

class OrchestratorMediaRouterModuleInterface : public OrchestratorModuleInterface
{
public:
	OrchestratorModuleType GetModuleType() const override
	{
		return OrchestratorModuleType::MediaRouter;
	}
};

class OrchestratorTranscoderModuleInterface : public OrchestratorModuleInterface
{
public:
	OrchestratorModuleType GetModuleType() const override
	{
		return OrchestratorModuleType::Transcoder;
	}
};

class OrchestratorPublisherModuleInterface : public OrchestratorModuleInterface
{
public:
	OrchestratorModuleType GetModuleType() const override
	{
		return OrchestratorModuleType::Publisher;
	}
};
