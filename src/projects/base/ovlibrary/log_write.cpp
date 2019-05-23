//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Benjamin
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================

#include <iostream>
#include <iomanip>
#include <memory>
#include <sys/stat.h>

#include "log_write.h"

#define OV_LOG_DIR      "logs"
#define OV_LOG_DIR_SVC  "/var/log/ovenmediaengine"
#define OV_LOG_FILE     "ovenmediaengine.log"

namespace ov
{
    bool LogWrite::_start_service = false;

    LogWrite::LogWrite() :
        _last_day(0),
        _log_path(OV_LOG_DIR),
        _log_file(_log_path + std::string("/") + std::string(OV_LOG_FILE))
    {
    }

    void LogWrite::SetLogPath(const char* log_path)
    {
        _log_path = log_path;
        _log_file = log_path + std::string("/") + std::string(OV_LOG_FILE);
    }

    void LogWrite::Initialize()
    {
        if (_start_service)
        {
            SetLogPath(OV_LOG_DIR_SVC);

            // Change default log path to /var once for running service
            _start_service = false;
        }

        if ((::mkdir(_log_path.c_str(), 0755) == -1) && errno != EEXIST)
        {
            return;
        }

        _log_stream.close();
        _log_stream.clear();
        _log_stream.open(_log_file, std::ofstream::out | std::ofstream::app);
    }

    void LogWrite::Initialize(bool start_service)
    {
        _start_service = start_service;
    }

    void LogWrite::Write(const char* log)
    {
        std::time_t time = std::time(nullptr);
        std::tm localTime {};
        ::localtime_r(&time, &localTime);

        if (_last_day != localTime.tm_mday)
        {
            if (_last_day)
            {
                std::ostringstream logfile;
                logfile << _log_file << "." << std::put_time(&localTime, "%Y%m%d");
                ::rename(_log_file.c_str(), logfile.str().c_str());
            }
            _last_day = localTime.tm_mday;
        }

        struct stat file_stat {};
        if (!_log_stream.is_open() || _log_stream.fail() || ::stat(_log_file.c_str(), &file_stat) != 0)
        {
            Initialize();
        }

        _log_stream << log << std::endl;
        _log_stream.flush();
    }
}