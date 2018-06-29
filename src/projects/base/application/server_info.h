//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Gil Hoon Choi
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include "base/common_types.h"
#include "host_info.h"

class ServerInfo
{
public:
    ServerInfo();
    virtual ~ServerInfo();

    const ov::String GetName() const noexcept;
    void SetName(ov::String name);

    std::vector<std::shared_ptr<HostInfo>> GetHosts() const noexcept;
    void AddHost(std::shared_ptr<HostInfo> host);

    ov::String ToString() const;

private:
    ov::String _name;

    std::vector<std::shared_ptr<HostInfo>> _hosts;
};
