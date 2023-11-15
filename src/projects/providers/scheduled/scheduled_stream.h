//==============================================================================
//
//  ScheduledStream
//
//  Created by Getroot
//  Copyright (c) 2023 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <modules/ffmpeg/ffmpeg_conv.h>
#include <base/provider/stream.h>
#include "schedule.h"

namespace pvd
{
    // ScheduledStream doesn't support multiple tracks yet
    constexpr int kScheduledVideoTrackId = 0;
    constexpr int kScheduledAudioTrackId = 1;
    constexpr int kScheduledDataTrackId = 2;
    constexpr int kScheduledTimebase = 1000;

    class ScheduledStream : public Stream
    {
    public:
        static std::shared_ptr<ScheduledStream> Create(const std::shared_ptr<Application> &application, const info::Stream &stream_info, const std::shared_ptr<Schedule> &schedule);

        explicit ScheduledStream(const std::shared_ptr<Application> &application, const info::Stream &info, const std::shared_ptr<Schedule> &schedule);
        ~ScheduledStream();

        bool Start() override;
        bool Stop() override;
        bool Terminate() override;

        bool UpdateSchedule(const std::shared_ptr<Schedule> &schedule);
        std::shared_ptr<const Schedule> PeekSchedule() const;

        // Get current program
        bool GetCurrentProgram(std::shared_ptr<Schedule::Program> &curr_program, std::shared_ptr<Schedule::Item> &curr_item, int64_t &curr_item_pos) const;

    private:
        void WorkerThread();

        enum class PlaybackResult
        {
            PLAY_NEXT_ITEM,
            PLAY_NEXT_PROGRAM,
            PLAY_FALLBACK,
            ERROR,
            STOP
        };

        PlaybackResult PlayFile(const std::shared_ptr<Schedule::Item> &item);
        AVFormatContext *PrepareFilePlayback(const std::shared_ptr<Schedule::Item> &item);

        PlaybackResult PlayStream(const std::shared_ptr<Schedule::Item> &item);
        PlaybackResult PlayFallback();

        std::shared_ptr<Schedule> GetSchedule() const;
        bool CheckCurrentProgramChanged();
        
        int FindTrackIdByAvstreamId(int avstream_id) const;

        std::shared_ptr<Schedule> _schedule;
        mutable std::shared_mutex _schedule_mutex;

        std::thread _worker_thread;
        bool _worker_thread_running = false;

        mutable ov::Event _schedule_updated;

        // Current
        const Schedule::Stream _channel_info;

        mutable std::shared_mutex _current_mutex;
        std::shared_ptr<Schedule> _current_schedule;
        std::shared_ptr<Schedule::Program> _current_program;
        std::shared_ptr<Schedule::Item> _current_item;
        int64_t _current_item_position_ms = 0;

        std::map<int, int> _avstream_id_track_id_map;

        std::map<int, int64_t> _last_track_duration_map;

        ov::StopWatch _realtime_clock;
    };
}