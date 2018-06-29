//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Gil Hoon Choi
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#include "server_info.h"

ServerInfo::ServerInfo()
{
}

ServerInfo::~ServerInfo()
{
}

const ov::String ServerInfo::GetName() const noexcept
{
    return _name;
}

void ServerInfo::SetName(ov::String name)
{
    _name = name;
}

std::vector<std::shared_ptr<HostInfo>> ServerInfo::GetHosts() const noexcept
{
    return _hosts;
}

void ServerInfo::AddHost(std::shared_ptr<HostInfo> host)
{
    _hosts.push_back(host);
}

ov::String ServerInfo::ToString() const
{
    ov::String result = ov::String::FormatString("{\"name\": \"%s\"", _name.CStr());

    if(_hosts.size() > 0)
    {
        result.Append(", \"hosts\": [");

        for(auto iterator = _hosts.begin(); iterator != _hosts.end(); ++iterator)
        {
            if(iterator != _hosts.begin())
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