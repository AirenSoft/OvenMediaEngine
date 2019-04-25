//
// Created by benjamin on 19. 4. 24.
//

#include <iostream>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <memory>
#include <sys/stat.h>

#include "log_write.h"

#define OV_LOG_DIR      "/var/log/ovenmediaengine/"
#define OV_LOG_FILE     OV_LOG_DIR "ovenmediaengine.log"

namespace ov
{
    LogWrite::LogWrite() : _last_day(0)
    {
        if ((::mkdir(OV_LOG_DIR, 0755) == -1) && errno != EEXIST)
        {
            std::cout << "Cannot create directory, errno=" << errno << std::endl;
            return;
        }

        _log_stream = std::make_shared<std::ofstream>();
        _log_stream->open(OV_LOG_FILE, std::ofstream::out | std::ofstream::app);
        if (_log_stream->good())
        {
            std::cout.rdbuf(_log_stream->rdbuf());
        }
    }

    void LogWrite::Write(const char* log)
    {
        if (_log_stream && _log_stream->good())
        {
            std::time_t time = std::time(nullptr);
            std::tm localTime {};
            ::localtime_r(&time, &localTime);

            if (_last_day != localTime.tm_mday)
            {
                if (_last_day)
                {
                    std::ostringstream logfile;
                    logfile << OV_LOG_DIR << std::put_time(&localTime, "%Y%m%d") << ".log";
                    ::rename(OV_LOG_FILE, logfile.str().c_str());
                }
                _last_day = localTime.tm_mday;
            }

            struct stat file_stat {};
            if (stat(OV_LOG_FILE, &file_stat) != 0)
            {
                _log_stream->close();
                _log_stream->open(OV_LOG_FILE, std::ofstream::out | std::ofstream::app);
            }
        }

        std::cout << log;
        std::cout.flush();
    }
}