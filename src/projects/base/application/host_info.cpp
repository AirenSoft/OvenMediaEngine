//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Gil Hoon Choi
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "host_info.h"

#include <random>

HostInfo::HostInfo()
{
    // ID RANDOM 생성
    std::random_device rd;
    std::mt19937 mt(rd());
    std::uniform_int_distribution<uint32_t> dist(1, UINT32_MAX);
    _id = dist(mt);
}

HostInfo::~HostInfo()
{
}

const uint32_t HostInfo::GetId() const noexcept
{
    return _id;
}

const ov::String HostInfo::GetName() const noexcept
{
    return _name;
}

void HostInfo::SetName(ov::String name)
{
    _name = name;
}

std::shared_ptr<HostProviderInfo> HostInfo::GetProvider() const noexcept
{
    return _provider;
}

void HostInfo::SetProvider(std::shared_ptr<HostProviderInfo> provider)
{
    _provider = provider;
}

std::shared_ptr<HostPublisherInfo> HostInfo::GetPublisher() const noexcept
{
    return _publisher;
}

void HostInfo::SetPublisher(std::shared_ptr<HostPublisherInfo> publisher)
{
    _publisher = publisher;
}

std::vector<std::shared_ptr<ApplicationInfo>> HostInfo::GetApplications() const noexcept
{
    return _applications;
}

void HostInfo::AddApplication(std::shared_ptr<ApplicationInfo> application)
{
    _applications.push_back(application);
}

const ov::String HostInfo::GetApplicationsRef() const noexcept
{
    return _applications_ref;
}

void HostInfo::SetApplicationsRef(ov::String applications_ref)
{
    _applications_ref = applications_ref;
}

ov::String HostInfo::ToString() const
{
    ov::String result = ov::String::FormatString("{\"id\": %ld, \"name\": \"%s\", \"ip_address\": \"%s\", \"max_connection\": %d, \"provider\": %s, \"publisher\": %s, \"applications_ref\": \"%s\"",
                                                _id, _name.CStr(), _ip_address.CStr(), _max_connection, _provider->ToString().CStr(), _publisher->ToString().CStr(), _applications_ref.CStr());

    if(_applications.size() > 0)
    {
        result.Append(", \"applications\": [");

        for(auto iterator = _applications.begin(); iterator != _applications.end(); ++iterator)
        {
            if(iterator != _applications.begin())
            {
                result.Append(",");
            }

            result.AppendFormat("%s", (*iterator)->ToString().CStr());
        }

        result.Append("]");
    }

    result.Append("}");

    return result;
}