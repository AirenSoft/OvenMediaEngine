//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Gil Hoon Choi
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "host_base_info.h"
#include "host_provider_info.h"
#include "host_publisher_info.h"
#include "application_info.h"

#include "base/common_types.h"

class HostInfo : public HostBaseInfo
{
public:
    HostInfo();
    virtual ~HostInfo();

    const uint32_t GetId() const noexcept;

    const ov::String GetName() const noexcept;
    void SetName(ov::String name);

    std::shared_ptr<HostProviderInfo> GetProvider() const noexcept;
    void SetProvider(std::shared_ptr<HostProviderInfo> provider);

    std::shared_ptr<HostPublisherInfo> GetPublisher() const noexcept;
    void SetPublisher(std::shared_ptr<HostPublisherInfo> publisher);

    std::vector<std::shared_ptr<ApplicationInfo>> GetApplications() const noexcept;
    void AddApplication(std::shared_ptr<ApplicationInfo> application);

    const ov::String GetApplicationsRef() const noexcept;
    void SetApplicationsRef(ov::String applications_ref);

    ov::String ToString() const;

private:
    uint32_t _id;
    ov::String _name;

    std::shared_ptr<HostProviderInfo> _provider;
    std::shared_ptr<HostPublisherInfo> _publisher;

    std::vector<std::shared_ptr<ApplicationInfo>> _applications;

    ov::String _applications_ref;
};
