//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Benjamin
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

namespace ov
{
    class LogWrite
    {
    public:
        LogWrite();
        virtual ~LogWrite() = default;
        void Write(const char* log);

    private:
        std::shared_ptr<std::ofstream> _log_stream;
        int _last_day;
    };
}

