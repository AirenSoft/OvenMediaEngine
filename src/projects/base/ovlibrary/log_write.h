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
#include <mutex>

#define OV_LOG_DIR              "logs"
#define OV_LOG_DIR_SVC          "/var/log/ovenmediaengine"
#define OV_DEFAULT_LOG_FILE     "ovenmediaengine.log"
namespace ov
{
    class LogWrite
    {
    public:
        LogWrite(std::string log_file_name);
        virtual ~LogWrite() = default;
        void Write(const char* log);
        void SetLogPath(const char* log_path);

        static void Initialize(bool start_service);

    private:
        void Initialize();

        std::mutex _log_stream_mutex;
        std::ofstream _log_stream;
        int _last_day;
        std::string _log_path;
        std::string _log_file_name;
        std::string _log_file;

        static bool _start_service;
    };
}

