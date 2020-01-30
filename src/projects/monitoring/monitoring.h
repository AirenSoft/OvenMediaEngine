//
// Created by getroot on 20. 1. 16.
//

#pragma once

#include "base/info/info.h"
#include "base/info/host.h"
#include "base/info/stream_info.h"

class Monitoring
{
public:
    void        OnHostCreate(const info::Host &host_info);
    void        OnHostDelete(const info::Host &host_info);
    void        OnApplicationCreated(const info::Application &app_info);
    void        OnApplicationDeleted(const info::Application &app_info);
    void        OnStreamCreated(const info::StreamInfo &stream_info);
    void        OnStreamDeleted(const info::StreamInfo &stream_info);
};

