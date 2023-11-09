//==============================================================================
//
//  ScheduledApplication
//
//  Created by Getroot
//  Copyright (c) 2023 AirenSoft. All rights reserved.
//
//==============================================================================

#include "scheduled_application.h"
#include "scheduled_provider_private.h"
#include "scheduled_stream.h"
#include "schedule.h"

#include <base/ovlibrary/files.h>

namespace pvd
{
    // Implementation of ScheduledApplication
    std::shared_ptr<ScheduledApplication> ScheduledApplication::Create(const std::shared_ptr<Provider> &provider, const info::Application &application_info)
    {
        auto application = std::make_shared<pvd::ScheduledApplication>(provider, application_info);
        if (!application->Start())
        {
            return nullptr;
        }

        return application;
    }

    ScheduledApplication::ScheduledApplication(const std::shared_ptr<Provider> &provider, const info::Application &info)
        : Application(provider, info)
    {
    }

    bool ScheduledApplication::Start()
    {
        bool parsed = false;
        auto config = GetConfig().GetProviders().GetScheduledProvider(&parsed);

        if (parsed == false ||
            config.GetRootDir().IsEmpty() || 
            config.GetScheduleFiles().IsEmpty())
        {
            logte("Could not create %s application for ScheduleProvider since invalid configuration", GetName().CStr());
            return false;
        }

        auto root_dir = config.GetRootDir();
        // Add trailing slash
        if (root_dir.HasSuffix('/') == false)
        {
            root_dir.Append('/');
        }

        // Absolute path
        if (root_dir.Get(0) == '/' || root_dir.Get(0) == '\\')
        {
            _root_dir = root_dir;
        }
        // Relative path
        else
        {
            // Get binary path
            auto binary_path = ov::GetBinaryPath();
            if (binary_path.HasSuffix('/') == false)
            {
                binary_path.Append('/');
            }

            _root_dir = binary_path + root_dir;
        }

        
        auto schedule_files = config.GetScheduleFiles();

        ov::String schedule_files_path;
        ov::String schedule_files_name;

        // split dir and file name from schedule_files
        auto index = schedule_files.IndexOfRev('/');
        if (index == -1)
        {
            schedule_files_name = schedule_files;
        }
        else
        {
            schedule_files_path = schedule_files.Substring(0, index);
            // Remove slash from the beginning
            if (schedule_files_path.HasPrefix('/'))
            {
                schedule_files_path = schedule_files_path.Substring(1);
            }

            schedule_files_name = schedule_files.Substring(index + 1);
        }

        _schedule_files_path = _root_dir + schedule_files_path;
        _schedule_files_regex = ov::Regex::CompiledRegex(ov::Regex::WildCardRegex(schedule_files_name));

        return Application::Start();
    }

    bool ScheduledApplication::Stop()
    {
        return Application::Stop();
    }

    // Called by ScheduledProvider thread every 500ms 
    void ScheduledApplication::OnScheduleWatcherTick()
    {
        // List all files in schedule file root
        auto schedule_file_info_list = SearchScheduleFileInfoFromDir(_schedule_files_path, _schedule_files_regex);

        // Check if file is added or updated
        for (auto &schedule_file_info : schedule_file_info_list)
        {
            ScheduleFileInfo db_schedule_file_info;
            if (GetScheduleFileInfoFromDB(schedule_file_info._file_path.Hash(), db_schedule_file_info) == false)
            {
                // New file
                AddSchedule(schedule_file_info);
                _schedule_file_info_db.emplace(schedule_file_info._file_path.Hash(), schedule_file_info);
            }
            else
            {
                // Updated?
                if (schedule_file_info._file_stat.st_mtime != db_schedule_file_info._file_stat.st_mtime)
                {
                    UpdateSchedule(db_schedule_file_info);
                    _schedule_file_info_db[schedule_file_info._file_path.Hash()] = schedule_file_info;
                }
            }
        }

        // Check if file is deleted
        for (auto it = _schedule_file_info_db.begin(); it != _schedule_file_info_db.end();)
        {
            auto &schedule_file_info = it->second;
            if (stat(schedule_file_info._file_path.CStr(), &schedule_file_info._file_stat) != 0)
            {
                // File is deleted
                RemoveSchedule(schedule_file_info);
                it = _schedule_file_info_db.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    std::vector<ScheduledApplication::ScheduleFileInfo> ScheduledApplication::SearchScheduleFileInfoFromDir(const ov::String &schedule_file_root, ov::Regex &schedule_file_name_regex) const
    {
        std::vector<ScheduleFileInfo> schedule_files;

        auto [success, file_list] = ov::GetFileList(schedule_file_root);
        if (success == false)
        {
            return schedule_files;
        }

        for (auto &file_path : file_list)
        {
            if (schedule_file_name_regex.Matches(file_path).IsMatched() == false)
            {
                continue;
            }

            ScheduleFileInfo schedule_file_info;
            schedule_file_info._file_path = file_path;

            if (stat(file_path.CStr(), &schedule_file_info._file_stat) != 0)
            {
                continue;
            }

            if (S_ISREG(schedule_file_info._file_stat.st_mode) == false)
            {
                continue;
            }

            logtd("Found schedule file : %s, mtime : %d, hash : %d", schedule_file_info._file_path.CStr(), schedule_file_info._file_stat.st_mtime, schedule_file_info._file_path.Hash());

            schedule_files.push_back(schedule_file_info);
        }

        return schedule_files;
    }

    bool ScheduledApplication::GetScheduleFileInfoFromDB(const size_t &hash, ScheduleFileInfo &schedule_file_info)
    {
        auto it = _schedule_file_info_db.find(hash);
        if (it == _schedule_file_info_db.end())
        {
            return false;
        }

        schedule_file_info = it->second;
        return true;
    }

    bool ScheduledApplication::AddSchedule(ScheduleFileInfo &schedule_file_info)
    {
        std::shared_ptr<Schedule> schedule = Schedule::Create(schedule_file_info._file_path, _root_dir);
        if (schedule == nullptr)
        {
            logte("Failed to add schedule (Could not create schedule): %s", schedule_file_info._file_path.CStr());
            return false;
        }

        // Create Stream
        auto stream_info = info::Stream(*this, IssueUniqueStreamId(), StreamSourceType::Scheduled);
        stream_info.SetName(schedule->GetStream().name);

        if (schedule->GetStream().bypass_transcoder == true)
        {
            stream_info.SetRepresentationType(StreamRepresentationType::Relay);
        }
        
        auto stream = ScheduledStream::Create(GetSharedPtrAs<Application>(), stream_info, schedule);
        if (stream == nullptr)
        {
            logte("Failed to add schedule (Could not create stream): %s", schedule_file_info._file_path.CStr());
            return false;
        }

        auto channel_info = schedule->GetStream();

        // Make temp tracks
        if (channel_info.video_track == true)
        {
            auto track = std::make_shared<MediaTrack>();
            track->SetId(kScheduledVideoTrackId);
            auto public_name = ov::String::FormatString("Video_%d", track->GetId());
            track->SetPublicName(public_name);
            track->SetMediaType(cmn::MediaType::Video);

            // Set Timebase to 1/1000 fixed
            track->SetTimeBase(1, 1000);

            stream->AddTrack(track);
        }

        if (channel_info.audio_track == true)
        {
            auto track = std::make_shared<MediaTrack>();
            track->SetId(kScheduledAudioTrackId);
            auto public_name = ov::String::FormatString("Audio_%d", track->GetId());
            track->SetPublicName(public_name);
            track->SetMediaType(cmn::MediaType::Audio);

            // Set Timebase to 1/1000 fixed
            track->SetTimeBase(1, 1000);

            stream->AddTrack(track);
        }

        if (AddStream(stream) == false)
        {
            logte("Failed to add schedule (Could not add stream): %s", schedule_file_info._file_path.CStr());
            return false;
        }

        stream->Start();

        // Add file info to DB 
        schedule_file_info._schedule = schedule;

        return true;
    }

    bool ScheduledApplication::UpdateSchedule(ScheduleFileInfo &schedule_file_info)
    {
        std::shared_ptr<Schedule> schedule = Schedule::Create(schedule_file_info._file_path, _root_dir);
        if (schedule == nullptr)
        {
            logtw("Failed to update schedule (Could not create schedule): %s", schedule_file_info._file_path.CStr());
            return false;
        }

        // Check stream name
        if (schedule_file_info._schedule->GetStream().name != schedule->GetStream().name)
        {
            logtw("Failed to update schedule (%s stream name could not be changed) : %s", schedule_file_info._file_path.CStr(), schedule->GetStream().name.CStr());
            return false;
        }

        auto stream = std::static_pointer_cast<ScheduledStream>(GetStreamByName(schedule->GetStream().name));
        if (stream == nullptr)
        {
            logtw("Failed to update schedule (%s stream not found ) : %s", schedule_file_info._file_path.CStr(), schedule->GetStream().name.CStr());
            return false;
        }

        if (stream->UpdateSchedule(schedule) == false)
        {
            logtw("Failed to update schedule (%s stream failed to update schedule) : %s", schedule_file_info._file_path.CStr(), schedule->GetStream().name.CStr());
            return false;
        }

        // Update file info to DB
        schedule_file_info._schedule = schedule;

        return true;
    }

    bool ScheduledApplication::RemoveSchedule(ScheduleFileInfo &schedule_file_info)
    {
        auto stream_name = schedule_file_info._schedule->GetStream().name;

        // Remove Stream
        auto stream = std::static_pointer_cast<ScheduledStream>(GetStreamByName(stream_name));
        if (stream == nullptr)
        {
            logte("Failed to remove schedule (Could not find %s stream): %s", stream_name.CStr(), schedule_file_info._file_path.CStr());
            return false;
        }

        if (DeleteStream(stream) == false)
        {
            logte("Failed to remove schedule (Could not delete %s stream): %s", stream_name.CStr(), schedule_file_info._file_path.CStr());
            return false;
        }

        return true;
    }
}
