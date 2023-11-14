//==============================================================================
//
//  ScheduledStream
//
//  Created by Getroot
//  Copyright (c) 2023 AirenSoft. All rights reserved.
//
//==============================================================================

#include "scheduled_stream.h"
#include "schedule_private.h"

#include <base/provider/application.h>

namespace pvd
{
    // Implementation of ScheduledStream
    std::shared_ptr<ScheduledStream> ScheduledStream::Create(const std::shared_ptr<Application> &application, const info::Stream &stream_info, const std::shared_ptr<Schedule> &schedule)
    {
        auto stream = std::make_shared<pvd::ScheduledStream>(application, stream_info, schedule);
        return stream;
    }

    ScheduledStream::ScheduledStream(const std::shared_ptr<Application> &application, const info::Stream &info, const std::shared_ptr<Schedule> &schedule)
        : Stream(application, info), _schedule(schedule), _channel_info(schedule->GetStream())
    {
    }

    ScheduledStream::~ScheduledStream()
    {
        Stop();
    }

    bool ScheduledStream::Start()
    {
        // Create Worker
        _worker_thread_running = true;
        _worker_thread = std::thread(&ScheduledStream::WorkerThread, this);
        pthread_setname_np(_worker_thread.native_handle(), "Scheduled");

        return Stream::Start();
    }

    bool ScheduledStream::Stop()
    {
        if (_worker_thread_running == false)
        {
            return true;
        }

        _worker_thread_running = false;
        _schedule_updated.SetEvent();

        if (_worker_thread.joinable())
        {
            _worker_thread.join();
        }

        return Stream::Stop();
    }

    bool ScheduledStream::Terminate()
    {
        return Stream::Terminate();
    }

    std::shared_ptr<Schedule> ScheduledStream::GetSchedule() const
    {
        std::shared_lock<std::shared_mutex> lock(_schedule_mutex);
        _schedule_updated.Reset();
        return _schedule;
    }

    bool ScheduledStream::UpdateSchedule(const std::shared_ptr<Schedule> &schedule)
    {
        std::lock_guard<std::shared_mutex> lock(_schedule_mutex);
        _schedule = schedule;

        _schedule_updated.SetEvent();
        return true;
    }

    bool ScheduledStream::CheckCurrentProgramChanged()
    {
        // Check if pointer is same
        if (_current_program == GetSchedule()->GetCurrentProgram())
        {
            return false;
        }

        if (_current_program == nullptr && GetSchedule()->GetCurrentProgram() != nullptr)
        {
            return true;
        }
        else if (_current_program != nullptr && GetSchedule()->GetCurrentProgram() == nullptr)
        {
            return true;
        }

        // even if the pointer is changed, but the program is the same, it is not changed
        if (*_current_program == *GetSchedule()->GetCurrentProgram())
        {
            return false;
        }

        return true;
    }

    void ScheduledStream::WorkerThread()
    {
        while (_worker_thread_running)
        {
            // Schedule
            _current_schedule = GetSchedule();
            if (_current_schedule == nullptr)
            {
                _realtime_clock.Pause();
                // Wait for schedule update
                _schedule_updated.Wait();
                continue;
            }

            // Programs
            _current_program = _current_schedule->GetCurrentProgram();
            if (_current_program == nullptr)
            {
                auto next_program = _current_schedule->GetNextProgram();
                if (next_program == nullptr)
                {
                    //TODO: Play fallback item (if exists)

                    // Wait for schedule update
                    logti("Scheduled Channel %s/%s: No program and no next program. Wait for schedule update", GetApplicationName(), GetName().CStr());
                    _realtime_clock.Pause();
                    _schedule_updated.Wait();
                    continue;
                }

                // Wait for next program
                auto now = std::chrono::system_clock::now();
                auto wait_time = std::chrono::duration_cast<std::chrono::milliseconds>(next_program->scheduled_time - now).count();
                if (wait_time > 0)
                {
                    // TODO: Play fallback item (if exists)
                    logti("Scheduled Channel %s/%s: No program. Wait for next program(%s : local : %s) %lld milliseconds", GetApplicationName(), GetName().CStr(), next_program->scheduled.CStr(), ov::Converter::ToISO8601String(next_program->scheduled_time).CStr(), wait_time);
                    _realtime_clock.Pause();

                    _schedule_updated.Wait(wait_time);
                }

                continue;
            }

            logti("Scheduled Channel %s/%s: Start %s program", GetApplicationName(), GetName().CStr(), _current_program->name.CStr());

            // Items
            int err_count = 0;
            while (_worker_thread_running)
            {
                if (CheckCurrentProgramChanged() == true)
                {
                    logti("Scheduled Channel %s/%s: Program changed", GetApplicationName(), GetName().CStr());
                    break;
                }

                _current_item = _current_program->GetNextItem();
                if (_current_item == nullptr)
                {
                    logti("Scheduled Channel %s/%s: Program ended", GetApplicationName(), GetName().CStr());
                    break;
                }
                
                PlaybackResult result = PlaybackResult::STOP;
                if (_current_item->file == true)
                {
                    result = PlayFile(_current_item);
                }
                else
                {
                    result = PlayStream(_current_item);
                }

                if (result == PlaybackResult::PLAY_NEXT_PROGRAM)
                {
                    err_count = 0;
                    break;
                }
                else if (result == PlaybackResult::PLAY_FALLBACK)
                {
                    //TODO : Play fallback item (if exists)
                }
                else if (result == PlaybackResult::STOP)
                {
                    logte("Scheduled Channel %s/%s: Playback stopped", GetApplicationName(), GetName().CStr());
                    _worker_thread_running = false;
                    break;
                }
                else if (result == PlaybackResult::ERROR)
                {
                    err_count ++;
                    if (err_count > 3)
                    {
                        auto sleep_ms = std::min(_current_item->duration_ms>0?_current_item->duration_ms:1000, (int64_t)1000);
                        logte("Scheduled Channel %s/%s: Too many errors. Sleep for %lld milliseconds", GetApplicationName(), GetName().CStr(), sleep_ms);
                        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
                    }
                }
                else
                {
                    // PLAY_NEXT_ITEM
                    err_count = 0;
                }
            }
        }
    }

    ScheduledStream::PlaybackResult ScheduledStream::PlayFile(const std::shared_ptr<Schedule::Item> &item)
    {
        logti("Scheduled Channel : %s/%s: Play file %s", GetApplicationName(), GetName().CStr(), item->url.CStr());

        ScheduledStream::PlaybackResult result = PlaybackResult::PLAY_NEXT_ITEM;

        auto context = PrepareFilePlayback(item);
        if (context == nullptr)
        {
            logte("Scheduled Channel : %s/%s: Failed to prepare file playback. Try to play next item", GetApplicationName(), GetName().CStr());
            return PlaybackResult::ERROR;
        }

        if (_realtime_clock.IsStart() == false)
        {
            _realtime_clock.Start();
        }

        if (_realtime_clock.IsPaused() == true)
        {
            _realtime_clock.Resume();
        }

        // Play
        AVPacket packet = { 0 };
        std::map<int, bool> track_first_packet_map;
        std::map<int, int64_t> track_single_file_dts_offset_map;
        std::map<int, bool> end_of_track_map;

        while (_worker_thread_running)
        {
            if (CheckCurrentProgramChanged() == true)
            {
                result = PlaybackResult::PLAY_NEXT_PROGRAM;
                break;
            }

            int32_t ret = ::av_read_frame(context, &packet);
            if (ret == AVERROR(EAGAIN))
            {
                logtw("Scheduled Channel : %s/%s: Failed to read frame. Error (%d, %s)", GetApplicationName(), GetName().CStr(), ret, "EAGAIN");
                continue;
            }
            else if (ret == AVERROR_EOF || ::avio_feof(context->pb))
            {
                // End of file
                logti("Scheduled Channel : %s/%s: End of file. Try to play next item", GetApplicationName(), GetName().CStr());
                result = PlaybackResult::PLAY_NEXT_ITEM;
                break;
            }
            else if (ret < 0)
            {
                char errbuf[AV_ERROR_MAX_STRING_SIZE] = { 0 };

                ::av_strerror(ret, errbuf, sizeof(errbuf));

                logte("%s/%s: Failed to read frame. Error (%d, %s). Try to play next item", GetApplicationName(), GetName().CStr(), ret, errbuf);

                result = PlaybackResult::PLAY_NEXT_ITEM;
                break;
            }

            auto track_id = FindTrackIdByAvstreamId(packet.stream_index);
            if (track_id < 0)
            {
                logtd("Scheduled Channel : %s/%s: Failed to find track %d", GetApplicationName(), GetName().CStr(), packet.stream_index);
                ::av_packet_unref(&packet);
                continue;
            }

            if (end_of_track_map.find(track_id) != end_of_track_map.end())
            {
                // End of track
                if (end_of_track_map.at(track_id) == true)
                {
                    ::av_packet_unref(&packet);
                    continue;
                }
            }
            else
            {
                end_of_track_map[track_id] = false;
            }

            auto track = GetTrack(track_id);
            if (track == nullptr)
            {
                logtw("Scheduled Channel : %s/%s: Failed to find track %d", GetApplicationName(), GetName().CStr(), track_id);
                ::av_packet_unref(&packet);
                continue;
            }

            cmn::BitstreamFormat bitstream_format = cmn::BitstreamFormat::Unknown;
			cmn::PacketType packet_type = cmn::PacketType::Unknown;
			switch (track->GetCodecId())
			{
				case cmn::MediaCodecId::H264:
					bitstream_format = cmn::BitstreamFormat::H264_AVCC;
					packet_type = cmn::PacketType::NALU;
					break;
                case cmn::MediaCodecId::H265:
                    bitstream_format = cmn::BitstreamFormat::HVCC;
                    packet_type = cmn::PacketType::NALU;
                    break;
				case cmn::MediaCodecId::Aac:
					bitstream_format = cmn::BitstreamFormat::AAC_RAW;
					packet_type = cmn::PacketType::RAW;
					break;
				case cmn::MediaCodecId::Opus:
					bitstream_format = cmn::BitstreamFormat::OPUS;
					packet_type = cmn::PacketType::RAW;
					break;
                case cmn::MediaCodecId::Mp3:
                    bitstream_format = cmn::BitstreamFormat::MP3;
                    packet_type = cmn::PacketType::RAW;
                    break;
				default:
                    logtw("Scheduled Channel : %s/%s: Unsupported codec %s", GetApplicationName(), GetName().CStr(), StringFromMediaCodecId(track->GetCodecId()).CStr());
					::av_packet_unref(&packet);
					continue;
			}

			auto media_packet = ffmpeg::Conv::ToMediaPacket(GetMsid(), track->GetId(), &packet, track->GetMediaType(), bitstream_format, packet_type);

            // Convert to fixed time base
            auto origin_tb = context->streams[packet.stream_index]->time_base;

            ::av_packet_unref(&packet);

            auto pts = media_packet->GetPts();
            auto dts = media_packet->GetDts();
            auto duration = media_packet->GetDuration();

            // origin timebase to track timebase
            pts = pts * track->GetTimeBase().GetTimescale() * origin_tb.num / origin_tb.den;
            dts = dts * track->GetTimeBase().GetTimescale() * origin_tb.num / origin_tb.den;
            duration = duration * track->GetTimeBase().GetTimescale() * origin_tb.num / origin_tb.den;

            if (track_first_packet_map.find(track_id) == track_first_packet_map.end())
            {
                track_first_packet_map[track_id] = true;
                track_single_file_dts_offset_map[track_id] = dts;
            }
            auto single_file_dts = dts - track_single_file_dts_offset_map[track_id];
            
            AdjustTimestampByBase(track_id, pts, dts, std::numeric_limits<int64_t>::max(), duration);

            media_packet->SetPts(pts);
            media_packet->SetDts(dts);
            media_packet->SetDuration(duration);

            logtd("Scheduled Channel Send Packet : %s/%s: Track %d, origin dts : %lld, pts %lld, dts %lld, duration %lld, tb %f", GetApplicationName(), GetName().CStr(), track_id, single_file_dts, pts, dts, duration, track->GetTimeBase().GetExpr());

            SendFrame(media_packet);

            _last_track_duration_map[track_id] = media_packet->GetDuration();

            // dts to real time (ms)
            auto single_file_dts_ms = single_file_dts * track->GetTimeBase().GetExpr() * 1000;

             // Get current play time
            if (item->duration_ms != -1)
            {
                if (single_file_dts_ms > item->duration_ms)
                {
                    end_of_track_map[track_id] = true;
                }

                // all tracks should be ended
                bool all_tracks_ended = true;
                for (auto &end_of_track : end_of_track_map)
                {
                    if (end_of_track.second == false)
                    {
                        all_tracks_ended = false;
                        break;
                    }
                }

                if (all_tracks_ended == true)
                {
                    // End of item
                    logti("Scheduled Channel : %s/%s: End of item (Current Pos : %.0f ms Duration : %lld ms). Try to play next item", GetApplicationName(), GetName().CStr(), single_file_dts_ms, item->duration_ms);
                    result = PlaybackResult::PLAY_NEXT_ITEM;
                    break;
                }
            }
            
            auto elapsed = _realtime_clock.Elapsed();
            auto dts_ms = (int64_t)(dts * 1000.0 * track->GetTimeBase().GetExpr());
            if (elapsed < dts_ms)
            {
                auto wait_time = dts_ms - elapsed;
                logtd("Scheduled Channel : %s/%s: Current(%lld) Dts(%lld) Wait(%lld)", GetApplicationName(), GetName().CStr(), elapsed, dts_ms, (int64_t)wait_time);
                std::this_thread::sleep_for(std::chrono::milliseconds(wait_time));
            }
        }

        ::avformat_close_input(&context);
        logti("Scheduled Channel : %s/%s: Playback stopped", GetApplicationName(), GetName().CStr());

        return result;
    }

    AVFormatContext *ScheduledStream::PrepareFilePlayback(const std::shared_ptr<Schedule::Item> &item)
    {
        _avstream_id_track_id_map.clear();

        AVFormatContext *format_context = nullptr;

        int err = 0;
        err = ::avformat_open_input(&format_context, item->url.CStr(), nullptr, nullptr);
        if (err < 0)
        {
            char errbuf[AV_ERROR_MAX_STRING_SIZE] = { 0 };

            ::av_strerror(err, errbuf, sizeof(errbuf));

            logte("%s/%s: Failed to open %s item. error (%d, %s)", GetApplicationName(), GetName().CStr(), item->url.CStr(), err, errbuf);
            return nullptr;
        }

        err = ::avformat_find_stream_info(format_context, nullptr);
        if (err < 0)
        {
            char errbuf[AV_ERROR_MAX_STRING_SIZE] = { 0 };

            ::av_strerror(err, errbuf, sizeof(errbuf));

            logte("%s/%s: Failed to find stream info. Error (%d, %s)", GetApplicationName(), GetName().CStr(), item->url.CStr(), err, errbuf);
            ::avformat_close_input(&format_context);
            return nullptr;
        }

        bool video_track_needed = _channel_info.video_track;
        bool audio_track_needed = _channel_info.audio_track;

        int64_t total_duration_ms = 0;

        for (uint32_t track_id = 0; track_id < format_context->nb_streams; track_id++)
        {
            if (video_track_needed == false && audio_track_needed == false)
            {
                break;
            }

            auto stream = format_context->streams[track_id];
            if (stream == nullptr)
            {
                continue;
            }

            if (ffmpeg::Conv::ToMediaType(stream->codecpar->codec_type) == cmn::MediaType::Video &&
                video_track_needed == true)
            {
                auto new_track = ffmpeg::Conv::CreateMediaTrack(stream);
                if (new_track == nullptr)
                {
                    continue;
                }

                new_track->SetId(kScheduledVideoTrackId);
                new_track->SetTimeBase(1, kScheduledTimebase); // We fixed time base in scheduled stream
                _avstream_id_track_id_map.emplace(stream->index, kScheduledVideoTrackId);
                UpdateTrack(new_track);

                if (total_duration_ms == 0)
                {
                    total_duration_ms = stream->duration * 1000 * ::av_q2d(stream->time_base);
                }
                else
                {  
                    total_duration_ms = std::min(total_duration_ms, (int64_t)(stream->duration * 1000 * ::av_q2d(stream->time_base)));
                }

                video_track_needed = false;
            }
            else if (ffmpeg::Conv::ToMediaType(stream->codecpar->codec_type) == cmn::MediaType::Audio &&
                audio_track_needed == true)
            {
                auto new_track = ffmpeg::Conv::CreateMediaTrack(stream);
                if (new_track == nullptr)
                {
                    continue;
                }

                new_track->SetId(kScheduledAudioTrackId);
                new_track->SetTimeBase(1, kScheduledTimebase); // We fixed time base in scheduled stream
                _avstream_id_track_id_map.emplace(stream->index, kScheduledAudioTrackId);
                UpdateTrack(new_track);

                if (total_duration_ms == 0)
                {
                    total_duration_ms = stream->duration * 1000 * ::av_q2d(stream->time_base);
                }
                else
                {  
                    total_duration_ms = std::min(total_duration_ms, (int64_t)(stream->duration * 1000 * ::av_q2d(stream->time_base)));
                }

                audio_track_needed = false;
            }
            else
            {
                continue;
            }
        }

        if (video_track_needed == true || audio_track_needed == true)
        {
            logte("%s/%s: Failed to find %s track(s) from file %s", GetApplicationName(), GetName().CStr(), 
                video_track_needed&& audio_track_needed == true ? "video and audio" :
                video_track_needed == true ? "video" : "audio", item->url.CStr());
            ::avformat_close_input(&format_context);
            return nullptr;
        }

        // If there is no data track, add a dummy data track
		if(GetFirstTrackByType(cmn::MediaType::Data) == nullptr)
		{
			auto data_track = std::make_shared<MediaTrack>();
			data_track->SetId(kScheduledDataTrackId);
			data_track->SetMediaType(cmn::MediaType::Data);
			data_track->SetTimeBase(1, 1000); // Data track time base is always 1/1000 in
			data_track->SetOriginBitstream(cmn::BitstreamFormat::ID3v2);
			
			UpdateTrack(data_track);
		}

        // Seek to start position
        if (item->start_time_ms > 0)
        {
            int64_t seek_target = item->start_time_ms * 1000;
            int64_t seek_min = 0;
            int64_t seek_max = total_duration_ms * 1000;

            int seek_ret = ::avformat_seek_file(format_context, -1, seek_min, seek_target, seek_max, 0);
            if (seek_ret < 0)
            {
                logte("%s/%s: Failed to seek to start position %d, err:%d", GetApplicationName(), GetName().CStr(), item->start_time_ms, seek_ret);
            }
        }

        if (UpdateStream() == false)
        {
            logte("%s/%s: Failed to update stream", GetApplicationName(), GetName().CStr());
            return nullptr;
        }

        return format_context;
    }

    ScheduledStream::PlaybackResult ScheduledStream::PlayStream(const std::shared_ptr<Schedule::Item> &item)
    {
        // Not yet supported
        return PlaybackResult::PLAY_NEXT_ITEM;
    }

    ScheduledStream::PlaybackResult ScheduledStream::PlayFallback()
    {
        // Not yet supported
        return PlaybackResult::PLAY_NEXT_ITEM;
    }

    int ScheduledStream::FindTrackIdByAvstreamId(int avstream_id) const
    {
        auto it = _avstream_id_track_id_map.find(avstream_id);
        if (it == _avstream_id_track_id_map.end())
        {
            return -1;
        }

        return it->second;
    }
}