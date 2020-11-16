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
#include <sstream>
#include <sys/stat.h>

#include "log_write.h"

namespace ov
{
    bool LogWrite::_start_service = false;

    LogWrite::LogWrite(std::string log_file_name) :
        _last_day(0),
        _log_path(OV_LOG_DIR)
    {
        if(log_file_name.empty())
        {
            _log_file_name = OV_LOG_FILE;
        }
        else
        {
            _log_file_name = log_file_name;
        }

        _log_file = _log_path + std::string("/") + log_file_name;
    }

    void LogWrite::SetLogPath(const char* log_path)
    {
        _log_path = log_path;
        _log_file = log_path + std::string("/") + _log_file_name;
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

        std::lock_guard<std::mutex> lock_guard(_log_stream_mutex);
        _log_stream.close();
        _log_stream.clear();
        _log_stream.open(_log_file, std::ofstream::out | std::ofstream::app);
    }

    void LogWrite::Initialize(bool start_service)
    {
        _start_service = start_service;
    }

    void LogWrite::Write(const char *log)
    {
        std::time_t time = std::time(nullptr);
        std::tm local_time {};
        ::localtime_r(&time, &local_time);

        // At the end of the day, change file name to back it up 
        // ovenmediaengine.log.YYmmDD
        if (_last_day != local_time.tm_mday)
        {
            if (_last_day)
            {
                std::ostringstream logfile;
                logfile << _log_file << "." << std::put_time(&local_time, "%Y%m%d");
                ::rename(_log_file.c_str(), logfile.str().c_str());
            }
            _last_day = local_time.tm_mday;
        }

        struct stat file_stat {};
        if (!_log_stream.is_open() || _log_stream.fail() || ::stat(_log_file.c_str(), &file_stat) != 0)
        {
            Initialize();
        }

        std::lock_guard<std::mutex> lock_guard(_log_stream_mutex);
        _log_stream << log << std::endl;
        _log_stream.flush();
    }
}