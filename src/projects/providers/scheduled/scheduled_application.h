//==============================================================================
//
//  ScheduledApplication
//
//  Created by Getroot
//  Copyright (c) 2023 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/common_types.h>
#include <base/provider/application.h>
#include <base/provider/stream.h>

#include "schedule.h"

constexpr const char* ScheduleFileExtension = "sch";
namespace pvd
{
    class ScheduledApplication : public Application
    {
    public:
        static std::shared_ptr<ScheduledApplication> Create(const std::shared_ptr<Provider> &provider, const info::Application &application_info);

        explicit ScheduledApplication(const std::shared_ptr<Provider> &provider, const info::Application &info);
        ~ScheduledApplication() override = default;

        bool Start() override;
        bool Stop() override;

        void OnScheduleWatcherTick();

        ov::String GetRootDir() const
        {
            return _media_root_dir;
        }

    private:
        struct ScheduleFileInfo
        {
            ov::String GetFileName()
            {
                // split by '/'
                auto items = _file_path.Split("/");
                if (items.empty())
                {
                    return "";
                }

                // get last item
                return items.back();
            }

            ov::String GetFileNameWithoutExt()
            {
                auto file_name = GetFileName();
                if (file_name.IsEmpty())
                {
                    return "";
                }

                // split by '.'
                auto items = file_name.Split(".");
                if (items.empty())
                {
                    return "";
                }

                // get first item
                return items.front();
            }

            ov::String _file_path;
            struct stat _file_stat;
            std::shared_ptr<Schedule> _schedule;

            uint32_t _deleted_checked_count = 0;
        };

        std::vector<ScheduleFileInfo> SearchScheduleFileInfoFromDir(const ov::String &schedule_file_root) const;

        bool GetScheduleFileInfoFromDB(const size_t &hash, ScheduleFileInfo &schedule_file_info);
        bool AddSchedule(ScheduleFileInfo &schedule_file_info);
        bool UpdateSchedule(ScheduleFileInfo &schedule_file_info, ScheduleFileInfo &new_schedule_file_info);
        bool RemoveSchedule(ScheduleFileInfo &schedule_file_info);

        ov::String _media_root_dir;
        ov::String _schedule_files_path;
        ov::Regex _schedule_file_name_regex;

        // File name hash -> ScheduleFileInfo
        std::map<size_t, ScheduleFileInfo> _schedule_file_info_db;
    };
}