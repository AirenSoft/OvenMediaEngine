//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Benjamin
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <fstream>

namespace ov
{
    class LogWrite
    {
    public:
        LogWrite();
        virtual ~LogWrite() = default;
        void Write(const char* log);
        void SetLogPath(const char* log_path);

        static void Initialize(bool start_service);

    private:
        void Initialize();

        std::ofstream _log_stream;
        int _last_day;
        std::string _log_path;
        std::string _log_file;

        static bool _start_service;
    };
}

