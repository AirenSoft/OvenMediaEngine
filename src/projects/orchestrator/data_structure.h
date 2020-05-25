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
	Unknown = 0x10000000,
	Provider = 0x00000001,
	MediaRouter = 0x00000010,
	Transcoder = 0x00000100,
	Publisher = 0x00001000
};

inline OrchestratorModuleType operator|(OrchestratorModuleType type1, OrchestratorModuleType type2)
{
	return static_cast<OrchestratorModuleType>(
		static_cast<std::underlying_type<OrchestratorModuleType>::type>(type1) |
		static_cast<std::underlying_type<OrchestratorModuleType>::type>(type2));
}

inline OrchestratorModuleType operator&(OrchestratorModuleType type1, OrchestratorModuleType type2)
{
	return static_cast<OrchestratorModuleType>(
		static_cast<std::underlying_type<OrchestratorModuleType>::type>(type1) &
		static_cast<std::underlying_type<OrchestratorModuleType>::type>(type2));
}

// Forward declaration
namespace pvd
{
	class Stream;
}

ov::String GetOrchestratorModuleTypeName(OrchestratorModuleType type);

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

	/// Called when another module is requested to pull stream list
	///
	/// @param app_info An information of the application
	/// @param stream_name A stream name to create
	/// @param url_list The streaming URLs to pull
	/// @param offset Specifies the starting point of the streaming URL (unit: milliseconds)
	///
	/// @return Newly created stream instance
	virtual std::shared_ptr<pvd::Stream> PullStream(const info::Application &app_info, const ov::String &stream_name, const std::vector<ov::String> &url_list, off_t offset) = 0;

	virtual bool StopStream(const info::Application &app_info, const std::shared_ptr<pvd::Stream> &stream) = 0;
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
