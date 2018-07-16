//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Gil Hoon Choi
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "provider_info.h"
#include "publisher_info.h"

#include "base/common_types.h"

enum class ApplicationType
{
	Unknown,
	Live,
	Vod,
	LiveEdge,
	VodEdge
};

class ApplicationInfo
{
public:
	ApplicationInfo();
	ApplicationInfo(const std::shared_ptr<ApplicationInfo> &info);
	virtual ~ApplicationInfo();

	const uint32_t GetId() const noexcept;

	const ov::String GetName() const noexcept;
	void SetName(ov::String name);

	const ApplicationType GetType() const noexcept;
	void SetType(ApplicationType type);
	void SetTypeFromString(const ov::String &type_string);

	std::vector<std::shared_ptr<PublisherInfo>> GetPublishers() const noexcept;
	void AddPublisher(std::shared_ptr<PublisherInfo> publisher_info);

	std::vector<std::shared_ptr<ProviderInfo>> GetProviders() const noexcept;
	void AddProvider(std::shared_ptr<ProviderInfo> provider);

	// Utilities
	static const char *StringFromApplicationType(ApplicationType application_type) noexcept;
	static const ApplicationType ApplicationTypeFromString(ov::String type_string) noexcept;

	ov::String ToString() const;

private:
	uint32_t _id;
	ov::String _name;
	ApplicationType _type;
	std::vector<std::shared_ptr<ProviderInfo>> _providers;
	std::vector<std::shared_ptr<PublisherInfo>> _publishers;
};
