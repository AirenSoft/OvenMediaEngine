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

    std::shared_ptr<const Schedule> ScheduledStream::PeekSchedule() const
    {
        std::shared_lock<std::shared_mutex> lock(_schedule_mutex);
        return _schedule;
    }

    std::shared_ptr<Schedule> ScheduledStream::GetSchedule() const
    {
        std::shared_lock<std::shared_mutex> lock(_schedule_mutex);
        _schedule_updated.Reset();
        return _schedule;
    }

    bool ScheduledStream::GetCurrentProgram(std::shared_ptr<Schedule::Program> &curr_program, std::shared_ptr<Schedule::Item> &curr_item, int64_t &curr_item_pos) const
    {
        std::shared_lock<std::shared_mutex> lock(_current_mutex);
        curr_program = _current_program;
        curr_item = _current_item;
        curr_item_pos = _current_item_position_ms;

        return true;
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
        std::shared_lock<std::shared_mutex> lock(_current_mutex);

        if (GetSchedule() == nullptr)
        {
            return true;
        }

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

    bool ScheduledStream::CheckCurrentFallbackProgramChanged()
    {
        // Check if pointer is same
        std::shared_lock<std::shared_mutex> lock(_current_mutex);

        if (GetSchedule() == nullptr)
        {
            return true;
        }

        if (_fallback_program == GetSchedule()->GetFallbackProgram())
        {
            return false;
        }

        if (_fallback_program == nullptr && GetSchedule()->GetFallbackProgram() != nullptr)
        {
            return true;
        }
        else if (_fallback_program != nullptr && GetSchedule()->GetFallbackProgram() == nullptr)
        {
            return true;
        }

        // even if the pointer is changed, but the program is the same, it is not changed
        if (*_fallback_program == *GetSchedule()->GetFallbackProgram())
        {
            return false;
        }

        return true;
    }

    bool ScheduledStream::CheckCurrentItemAvailable()
    {
        if (_current_item == nullptr)
        {
            return false;
        }

        // check every 3 seconds
        if (_failback_check_clock.IsStart() == false)
        {
            _failback_check_clock.Start();
        }

        if (_failback_check_clock.Elapsed() < 3000)
        {
            return false;
        }

        _failback_check_clock.Stop();

        if (_current_item->file == true)
        {
            return CheckFileItemAvailable(_current_item);
        }
        else
        {
            return CheckStreamItemAvailable(_current_item);
        }

        return false;
    }

    void ScheduledStream::WorkerThread()
    {
        while (_worker_thread_running)
        {
            // Schedule
            std::unique_lock<std::shared_mutex> guard(_current_mutex);
            _current_schedule = GetSchedule();
            _current_program = nullptr;
            _current_item = nullptr;
            _current_item_position_ms = 0;
            guard.unlock();

            if (_current_schedule == nullptr)
            {
                _realtime_clock.Pause();
                // Wait for schedule update
                _schedule_updated.Wait();
                continue;
            }

            // Programs
            guard.lock();
            _current_program = _current_schedule->GetCurrentProgram();
            _fallback_program = _current_schedule->GetFallbackProgram();
            guard.unlock();
            if (_current_program == nullptr)
            {
                PlayFallbackOrWait();
                continue;
            }

            logti("Scheduled Channel %s/%s: Start %s program", GetApplicationName(), GetName().CStr(), _current_program->name.CStr());

            // Items
            int err_count = 0;
            while (_worker_thread_running)
            {
                guard.lock();
                _current_item = nullptr;
                guard.unlock();
                
                if (CheckCurrentProgramChanged() == true)
                {
                    logti("Scheduled Channel %s/%s: Program changed", GetApplicationName(), GetName().CStr());
                    break;
                }

                guard.lock();
                _current_item = _current_program->GetNextItem();
                guard.unlock();
                if (_current_item == nullptr)
                {
                    logti("Scheduled Channel %s/%s: Program ended", GetApplicationName(), GetName().CStr());
                    PlayFallbackOrWait();
                    break;
                }
                
                bool need_break = false;
                do
                {
                    auto result = PlayItem(_current_item);
                    if (result == PlaybackResult::PLAY_NEXT_ITEM)
                    {
                        // PLAY_NEXT_ITEM
                        err_count = 0;
                    }
                    else if (result == PlaybackResult::PLAY_NEXT_PROGRAM)
                    {
                        err_count = 0;
                        need_break = true;
                        break;
                    }
                    else if (result == PlaybackResult::ERROR)
                    {
                        if (_current_item->fallback_on_err == true)
                        {
                            if (PlayFallbackOrWait() == PlaybackResult::FAILBACK)
                            {
                                continue;
                            }

							need_break = true;
							break;
                        }
                        // Play next item
                        else
                        {
                            _realtime_clock.Pause();

                            // Too many errors
                            err_count ++;
                            if (err_count > 3)
                            {
                                logte("Scheduled Channel %s/%s: Too many errors. Sleep for %lld milliseconds", GetApplicationName(), GetName().CStr(), 1000);
                                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                            }
                        }
                    }
                } while (0);

                if (need_break == true)
                {
                    break;
                }
            }
        }
    }

    ScheduledStream::PlaybackResult ScheduledStream::PlayFallbackOrWait()
    {
        logti("Scheduled Channel %s/%s: Start fallback program", GetApplicationName(), GetName().CStr());

        PlaybackResult result = PlaybackResult::PLAY_NEXT_ITEM;

        // Fallback
        while (_worker_thread_running)
        {
            if (CheckCurrentProgramChanged() == true)
            {
                logti("Scheduled Channel %s/%s: Program changed", GetApplicationName(), GetName().CStr());
                break;
            }

            if (CheckCurrentFallbackProgramChanged() == true)
            {
                logti("Scheduled Channel %s/%s: Fallback program changed", GetApplicationName(), GetName().CStr());
                break;
            }

            if (_fallback_program == nullptr || _fallback_program->items.empty() == true)
            {
                if (CheckCurrentItemAvailable() == true)
                {
                    return PlaybackResult::FAILBACK;
                }

                _realtime_clock.Pause();
                _schedule_updated.Wait(1000);

                continue;
            }

            auto item = _fallback_program->GetNextItem();
            result = PlayItem(item, true);
            if (result == PlaybackResult::FAILBACK)
            {
                break;
            }
            else if (result == PlaybackResult::PLAY_NEXT_ITEM)
            {
                
            }
            else if (result == PlaybackResult::PLAY_NEXT_PROGRAM)
            {
                break;
            }
            else if (result == PlaybackResult::ERROR)
            {
                logtw("Scheduled Channel %s/%s: Failed to play fallback program.", GetApplicationName(), GetName().CStr());
            }
        }

        return result;
    }

    ScheduledStream::PlaybackResult ScheduledStream::PlayItem(const std::shared_ptr<Schedule::Item> &item, bool fallback_item)
    {
        if (item == nullptr)
        {
            return PlaybackResult::ERROR;
        }

        if (item->file == true)
        {
            return PlayFile(item, fallback_item);
        }
        
        return PlayStream(item, fallback_item);
    }

    ScheduledStream::PlaybackResult ScheduledStream::PlayFile(const std::shared_ptr<Schedule::Item> &item, bool fallback_item)
    {
        logti("Scheduled Channel : %s/%s: Play file %s", GetApplicationName(), GetName().CStr(), item->file_path.CStr());

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

        bool is_mpegts { std::strncmp(context->iformat->name, "mpegts", 6) == 0 };

        while (_worker_thread_running)
        {
            if (CheckCurrentProgramChanged() == true)
            {
                result = PlaybackResult::PLAY_NEXT_PROGRAM;
                break;
            }

            if (fallback_item)
            {
                if (CheckCurrentItemAvailable() == true)
                {
                    result = PlaybackResult::FAILBACK;
                    break;
                }
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

            auto track_id = FindTrackIdByOriginId(packet.stream_index);
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
					bitstream_format = (is_mpegts) ? cmn::BitstreamFormat::H264_ANNEXB : cmn::BitstreamFormat::H264_AVCC;
					packet_type = cmn::PacketType::NALU;
					break;
				case cmn::MediaCodecId::H265:
					bitstream_format = (is_mpegts) ? cmn::BitstreamFormat::H265_ANNEXB : cmn::BitstreamFormat::HVCC;
					packet_type = cmn::PacketType::NALU;
					break;
				case cmn::MediaCodecId::Aac:
					bitstream_format = (is_mpegts) ? cmn::BitstreamFormat::AAC_ADTS : cmn::BitstreamFormat::AAC_RAW;
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
            pts = static_cast<double>(pts) * (static_cast<double>(origin_tb.num) / static_cast<double>(origin_tb.den) * track->GetTimeBase().GetTimescale());
            dts = static_cast<double>(dts) * (static_cast<double>(origin_tb.num) / static_cast<double>(origin_tb.den) * track->GetTimeBase().GetTimescale());
            duration = static_cast<double>(duration) * (static_cast<double>(origin_tb.num) / static_cast<double>(origin_tb.den) * track->GetTimeBase().GetTimescale());

            if (track_first_packet_map.find(track_id) == track_first_packet_map.end())
            {
                track_first_packet_map[track_id] = true;
                track_single_file_dts_offset_map[track_id] = dts;
            }
            auto single_file_dts = dts - track_single_file_dts_offset_map[track_id];
           
            AdjustTimestampByBase(track_id, pts, dts, std::numeric_limits<int64_t>::max(), duration);
			logtd("Scheduled Channel Send Packet : %s/%s: Track %d, origin dts : %lld, pts %lld, dts %lld, duration %lld, tb %f", GetApplicationName(), GetName().CStr(), track_id, single_file_dts, pts, dts, duration, track->GetTimeBase().GetExpr());

            media_packet->SetPts(pts);
            media_packet->SetDts(dts);
            media_packet->SetDuration(-1); // Duration will be calculated in MediaRouter

			double time_ms = static_cast<double>(dts) * track->GetTimeBase().GetExpr() * 1000.0;

            int64_t dts_gap = 0;
            if (_last_packet_map.find(track_id) != _last_packet_map.end())
            {
                auto last_packet = _last_packet_map.at(track_id);
                dts_gap = media_packet->GetDts() - last_packet->GetDts();
            }

            logtd("Scheduled Channel Send Packet : %s/%s: Track %d, origin dts : %lld, pts %lld, dts %lld, duration %lld, tb %f, dts_ms %f, dts_gap %lld", GetApplicationName(), GetName().CStr(), track_id, single_file_dts, pts, dts, duration, track->GetTimeBase().GetExpr(), time_ms, dts_gap);

            SendFrame(media_packet);

            _last_packet_map[track_id] = media_packet;

            // dts to real time (ms)
            auto single_file_dts_ms = static_cast<double>(single_file_dts) * track->GetTimeBase().GetExpr() * 1000.0;
			auto single_file_duration_ms = single_file_dts_ms + static_cast<double>(duration) * track->GetTimeBase().GetExpr() * 1000.0;

            std::unique_lock<std::shared_mutex> lock(_current_mutex);
            _current_item_position_ms = single_file_duration_ms;
            lock.unlock();

             // Get current play time
            if (item->duration_ms >= 0)
            {
                if (single_file_duration_ms >= item->duration_ms)
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
                    logti("Scheduled Channel : %s/%s: End of item (Current Pos : %.0f ms Duration : %lld ms). Try to play next item", GetApplicationName(), GetName().CStr(), single_file_duration_ms, item->duration_ms);
                    result = PlaybackResult::PLAY_NEXT_ITEM;
                    break;
                }
            }
            
            double elapsed = _realtime_clock.ElapsedUs();
            double dts_us = static_cast<double>(dts) * 1000.0 * 1000.0 * track->GetTimeBase().GetExpr();
            if (elapsed < dts_us)
            {
                int64_t wait_time = dts_us - elapsed;
                logtd("Scheduled Channel : %s/%s: Current(%f) Dts(%f) Wait(%lld)", GetApplicationName(), GetName().CStr(), elapsed, dts_us, wait_time);
                std::this_thread::sleep_for(std::chrono::microseconds(wait_time));
            }
        }

        _current_item_position_ms = 0;
        ::avformat_close_input(&context);
        logti("Scheduled Channel : %s/%s: Playback stopped", GetApplicationName(), GetName().CStr());

        return result;
    }

    bool ScheduledStream::CheckFileItemAvailable(const std::shared_ptr<Schedule::Item> &item)
    {
        // check file exists
        struct stat statbuf;
        if (stat(item->file_path.CStr(), &statbuf) != 0)
        {
            logte("%s/%s: Failed to find %s item. error (%d, %s)", GetApplicationName(), GetName().CStr(), item->file_path.CStr(), errno, strerror(errno));
            return false;
        }

        AVFormatContext *format_context = nullptr;

        int err = 0;
        err = ::avformat_open_input(&format_context, item->file_path.CStr(), nullptr, nullptr);
        if (err < 0)
        {
            char errbuf[AV_ERROR_MAX_STRING_SIZE] = { 0 };

            ::av_strerror(err, errbuf, sizeof(errbuf));

            logte("%s/%s: Failed to open %s item. error (%d, %s)", GetApplicationName(), GetName().CStr(), item->file_path.CStr(), err, errbuf);
            return false;
        }

        err = ::avformat_find_stream_info(format_context, nullptr);
        if (err < 0)
        {
            char errbuf[AV_ERROR_MAX_STRING_SIZE] = { 0 };

            ::av_strerror(err, errbuf, sizeof(errbuf));

            logte("%s/%s: Failed to find stream info. Error (%d, %s)", GetApplicationName(), GetName().CStr(), item->file_path.CStr(), err, errbuf);
            ::avformat_close_input(&format_context);
            return false;
        }

        bool video_track_needed = _channel_info.video_track;
        bool audio_track_needed = _channel_info.audio_track;

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
                video_track_needed = false;
            }
            else if (ffmpeg::Conv::ToMediaType(stream->codecpar->codec_type) == cmn::MediaType::Audio &&
                audio_track_needed == true)
            {
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
                video_track_needed == true ? "video" : "audio", item->file_path.CStr());
            ::avformat_close_input(&format_context);
            return false;
        }

        ::avformat_close_input(&format_context);

        return true;
    }

    AVFormatContext *ScheduledStream::PrepareFilePlayback(const std::shared_ptr<Schedule::Item> &item)
    {
        AVFormatContext *format_context = nullptr;

        int err = 0;
        err = ::avformat_open_input(&format_context, item->file_path.CStr(), nullptr, nullptr);
        if (err < 0)
        {
            char errbuf[AV_ERROR_MAX_STRING_SIZE] = { 0 };

            ::av_strerror(err, errbuf, sizeof(errbuf));

            logte("%s/%s: Failed to open %s item. error (%d, %s)", GetApplicationName(), GetName().CStr(), item->file_path.CStr(), err, errbuf);
            return nullptr;
        }

        err = ::avformat_find_stream_info(format_context, nullptr);
        if (err < 0)
        {
            char errbuf[AV_ERROR_MAX_STRING_SIZE] = { 0 };

            ::av_strerror(err, errbuf, sizeof(errbuf));

            logte("%s/%s: Failed to find stream info. Error (%d, %s)", GetApplicationName(), GetName().CStr(), item->file_path.CStr(), err, errbuf);
            ::avformat_close_input(&format_context);
            return nullptr;
        }

        bool video_track_needed = _channel_info.video_track;
        bool audio_track_needed = _channel_info.audio_track;

		uint32_t audio_index = 0;
        int64_t total_duration_ms = 0;
        _origin_id_track_id_map.clear();
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

				auto old_track = GetTrack(kScheduledVideoTrackId);

                new_track->SetId(kScheduledVideoTrackId);
                new_track->SetTimeBase(1, kScheduledVideoTimebase);
				new_track->SetPublicName(old_track->GetPublicName());
                _origin_id_track_id_map.emplace(stream->index, kScheduledVideoTrackId);
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

				auto audio_track_id = kScheduledAudioTrackId + audio_index;
				audio_index++;
				auto old_track = GetTrack(audio_track_id);

                new_track->SetId(audio_track_id);
                new_track->SetTimeBase(1, new_track->GetSampleRate());
				new_track->SetPublicName(old_track->GetPublicName());
				new_track->SetLanguage(old_track->GetLanguage());
				new_track->SetCharacteristics(old_track->GetCharacteristics());
                _origin_id_track_id_map.emplace(stream->index, audio_track_id);
                UpdateTrack(new_track);

                if (total_duration_ms == 0)
                {
                    total_duration_ms = stream->duration * 1000 * ::av_q2d(stream->time_base);
                }
                else
                {  
                    total_duration_ms = std::min(total_duration_ms, (int64_t)(stream->duration * 1000 * ::av_q2d(stream->time_base)));
                }
				
				if (audio_index + 1 > _channel_info.audio_map.size())
				{
                	audio_track_needed = false;
				}
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
                video_track_needed == true ? "video" : "audio", item->file_path.CStr());
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
			data_track->SetOriginBitstream(cmn::BitstreamFormat::Unknown);
			
			UpdateTrack(data_track);
		}

        if (item->duration_ms == 0)
        {
            item->duration_ms = total_duration_ms;
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

    ScheduledStream::PlaybackResult ScheduledStream::PlayStream(const std::shared_ptr<Schedule::Item> &item, bool fallback_item)
    {
        logti("Scheduled Channel : %s/%s: Play stream %s", GetApplicationName(), GetName().CStr(), item->url.CStr());

        ScheduledStream::PlaybackResult result = PlaybackResult::PLAY_NEXT_ITEM;

        auto stream_tap = PrepareStreamPlayback(item);
        if (stream_tap == nullptr)
        {
            logte("Scheduled Channel : %s/%s: Failed to prepare stream playback. Try to play next item", GetApplicationName(), GetName().CStr());
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

        std::map<int, bool> track_first_packet_map;
        std::map<int, int64_t> track_single_file_dts_offset_map;
        std::map<int, bool> end_of_track_map;

        // Play
        while (_worker_thread_running)
        {
            if (CheckCurrentProgramChanged() == true)
            {
                result = PlaybackResult::PLAY_NEXT_PROGRAM;
                break;
            }

            if (fallback_item)
            {
                if (CheckCurrentItemAvailable() == true)
                {
                    result = PlaybackResult::FAILBACK;
                    break;
                }
            }

            auto media_packet = stream_tap->Pop(500);
            if (media_packet == nullptr)
            {
                if (CheckCurrentProgramChanged() == true)
                {
                    result = PlaybackResult::PLAY_NEXT_PROGRAM;
                    break;
                }
                else
                {
                    logtw("Scheduled Channel : %s/%s: Failed to pop packet", GetApplicationName(), GetName().CStr());
                    result = PlaybackResult::ERROR;
                    break;
                }
            }
			
			auto origin_track_id = media_packet->GetTrackId();
            auto track_id = FindTrackIdByOriginId(origin_track_id);
            if (track_id < 0)
            {
                logtd("Scheduled Channel : %s/%s: Failed to find track %d", GetApplicationName(), GetName().CStr(), media_packet->GetTrackId());
                continue;
            }

            if (end_of_track_map.find(track_id) != end_of_track_map.end())
            {
                // End of track
                if (end_of_track_map.at(track_id) == true)
                {
                    continue;
                }
            }
            else
            {
                end_of_track_map[track_id] = false;
            }

            // Transcoder will make bogus frame if it can't be decoded
            // if (sent_keyframe == false && media_packet->GetMediaType() == cmn::MediaType::Video)
            // {
            //     if (media_packet->GetFlag() != MediaPacketFlag::Key)
            //     {
            //         // Skip until key frame
            //         continue;
            //     }
            //     else
            //     {
            //         sent_keyframe = true;
            //     }
            // }

            auto track = GetTrack(track_id);
            if (track == nullptr)
            {
                logtw("Scheduled Channel : %s/%s: Failed to find track %d", GetApplicationName(), GetName().CStr(), track_id);
                continue;
            }

            auto origin_tb = stream_tap->GetStreamInfo()->GetTrack(origin_track_id)->GetTimeBase();

            media_packet->SetTrackId(track_id);
            auto pts = media_packet->GetPts();
            auto dts = media_packet->GetDts();
            //auto duration = media_packet->GetDuration();

            // origin timebase to track timebase
            pts = ((pts * (double)origin_tb.GetNum()) / (double)origin_tb.GetDen()) * track->GetTimeBase().GetTimescale();
            dts = ((dts * (double)origin_tb.GetNum()) / (double)origin_tb.GetDen()) * track->GetTimeBase().GetTimescale();
            //duration = ((duration * (double)origin_tb.GetNum()) / (double)origin_tb.GetDen()) * track->GetTimeBase().GetTimescale();

            if (track_first_packet_map.find(track_id) == track_first_packet_map.end())
            {
                track_first_packet_map[track_id] = true;
                track_single_file_dts_offset_map[track_id] = dts;
            }
            auto single_file_dts = dts - track_single_file_dts_offset_map[track_id];

            //TODO(Getroot): need to calc duration?
            AdjustTimestampByBase(track_id, pts, dts, std::numeric_limits<int64_t>::max());
			
			media_packet->SetMsid(GetMsid());
            media_packet->SetPts(pts);
            media_packet->SetDts(dts);
			media_packet->SetDuration(-1); // It will be calculated in MediaRouter

			double time_ms = (double)(dts * 1000.0 * track->GetTimeBase().GetExpr());

            logtd("Scheduled Channel Send Packet : %s/%s: Track %d, origin dts : %lld, pts %lld, dts %lld, tb %f, dts_ms %f", GetApplicationName(), GetName().CStr(), track_id, single_file_dts, pts, dts, track->GetTimeBase().GetExpr(), time_ms);

            SendFrame(media_packet);

            // dts to real time (ms)
            auto single_file_dts_ms = static_cast<double>(single_file_dts) * track->GetTimeBase().GetExpr() * static_cast<double>(1000);

            std::unique_lock<std::shared_mutex> lock(_current_mutex);
            _current_item_position_ms = single_file_dts_ms;
            lock.unlock();

            // Get current play time
            if (item->duration_ms >= 0)
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
        }

        _current_item_position_ms = 0;

        ocst::Orchestrator::GetInstance()->UnmirrorStream(stream_tap);

        logti("Scheduled Channel : %s/%s: Playback stopped", GetApplicationName(), GetName().CStr());

        return result;
    }

    bool ScheduledStream::CheckStreamItemAvailable(const std::shared_ptr<Schedule::Item> &item)
    {
        auto stream_url = ov::Url::Parse(item->url);
        if (stream_url == nullptr)
        {
            logte("Scheduled Channel : %s/%s: Failed to parse stream url %s", GetApplicationName(), GetName().CStr(), item->url.CStr());
            return false;
        }

        auto vhost_app_name = info::VHostAppName(stream_url->Host(), stream_url->App());
        if (ocst::Orchestrator::GetInstance()->CheckIfStreamExist(vhost_app_name, stream_url->Stream()) == false)
        {
            logte("Scheduled Channel : %s/%s: Failed to find stream %s", GetApplicationName(), GetName().CStr(), item->url.CStr());
            return false;
        }

        auto stream_tap = MediaRouterStreamTap::Create();

        auto result = ocst::Orchestrator::GetInstance()->MirrorStream(stream_tap, vhost_app_name, stream_url->Stream(), MediaRouterInterface::MirrorPosition::Inbound);
        if (result != CommonErrorCode::SUCCESS)
        {
            logte("Scheduled Channel : %s/%s: Failed to mirror stream %s (err : %d)", GetApplicationName(), GetName().CStr(), item->url.CStr(), static_cast<int>(result));
            return false;
        }

        if (stream_tap->GetStreamInfo() == nullptr)
        {
            logte("Scheduled Channel : %s/%s: Failed to get stream info from stream tap %s", GetApplicationName(), GetName().CStr(), item->url.CStr());
            return false;
        }

        bool video_track_needed = _channel_info.video_track;
        bool audio_track_needed = _channel_info.audio_track;
        for (const auto &[track_id, track] : stream_tap->GetStreamInfo()->GetTracks())
        {
            if (video_track_needed == false && audio_track_needed == false)
            {
                break;
            }

            if (track->GetMediaType() == cmn::MediaType::Video &&
                video_track_needed == true)
            {
                video_track_needed = false;
            }
            else if (track->GetMediaType() == cmn::MediaType::Audio &&
                audio_track_needed == true)
            {
                audio_track_needed = false;
            }
            else
            {
                continue;
            }
        }

        if (video_track_needed == true || audio_track_needed == true)
        {
            logte("%s/%s: Failed to find %s track(s) from stream %s", GetApplicationName(), GetName().CStr(),
                video_track_needed&& audio_track_needed == true ? "video and audio" :
                video_track_needed == true ? "video" : "audio", item->url.CStr());

            ocst::Orchestrator::GetInstance()->UnmirrorStream(stream_tap);
            
            return false;
        }

        ocst::Orchestrator::GetInstance()->UnmirrorStream(stream_tap);
        return true;
    }

    std::shared_ptr<MediaRouterStreamTap> ScheduledStream::PrepareStreamPlayback(const std::shared_ptr<Schedule::Item> &item)
    {
        auto stream_tap = MediaRouterStreamTap::Create();
		stream_tap->SetNeedPastData(true);

        auto stream_url = ov::Url::Parse(item->url);
        if (stream_url == nullptr)
        {
            logte("Scheduled Channel : %s/%s: Failed to parse stream url %s", GetApplicationName(), GetName().CStr(), item->url.CStr());
            return nullptr;
        }

        auto vhost_app_name = info::VHostAppName(stream_url->Host(), stream_url->App());

        auto result = ocst::Orchestrator::GetInstance()->MirrorStream(stream_tap, vhost_app_name, stream_url->Stream(), MediaRouterInterface::MirrorPosition::Inbound);
        if (result != CommonErrorCode::SUCCESS)
        {
            logte("Scheduled Channel : %s/%s: Failed to mirror stream %s (err : %d)", GetApplicationName(), GetName().CStr(), item->url.CStr(), static_cast<int>(result));
            return nullptr;
        }

        if (stream_tap->GetStreamInfo() == nullptr)
        {
            logte("Scheduled Channel : %s/%s: Failed to get stream info from stream tap %s", GetApplicationName(), GetName().CStr(), item->url.CStr());
            ocst::Orchestrator::GetInstance()->UnmirrorStream(stream_tap);
            return nullptr;
        }

        _origin_id_track_id_map.clear();
        bool video_track_needed = _channel_info.video_track;
        bool audio_track_needed = _channel_info.audio_track;
		uint32_t audio_index = 0;
        for (const auto &[track_id, track] : stream_tap->GetStreamInfo()->GetTracks())
        {
            if (video_track_needed == false && audio_track_needed == false)
            {
                break;
            }

            if (track->GetMediaType() == cmn::MediaType::Video &&
                video_track_needed == true)
            {
                auto new_track = std::make_shared<MediaTrack>(*track);
                if (new_track == nullptr)
                {
                    continue;
                }

				auto old_track = GetTrack(kScheduledVideoTrackId);

                new_track->SetId(kScheduledVideoTrackId);
                new_track->SetTimeBase(1, kScheduledVideoTimebase);
				new_track->SetPublicName(old_track->GetPublicName());
                _origin_id_track_id_map.emplace(track_id, kScheduledVideoTrackId);
                UpdateTrack(new_track);

                video_track_needed = false;
            }
            else if (track->GetMediaType() == cmn::MediaType::Audio &&
                audio_track_needed == true)
            {
                auto new_track = std::make_shared<MediaTrack>(*track);
                if (new_track == nullptr)
                {
                    continue;
                }
				
				auto audio_track_id = kScheduledAudioTrackId + audio_index;
				audio_index++;
				auto old_track = GetTrack(audio_track_id);

                new_track->SetId(audio_track_id);
                new_track->SetTimeBase(1, new_track->GetSampleRate());
				new_track->SetPublicName(old_track->GetPublicName());
				new_track->SetLanguage(old_track->GetLanguage());
				new_track->SetCharacteristics(old_track->GetCharacteristics());
                _origin_id_track_id_map.emplace(track_id, audio_track_id);
                UpdateTrack(new_track);
				
				if (audio_index + 1 > _channel_info.audio_map.size())
				{
                	audio_track_needed = false;
				}
            }
            else
            {
                continue;
            }
        }

        if (video_track_needed == true || audio_track_needed == true)
        {
            logte("%s/%s: Failed to find %s track(s) from stream %s", GetApplicationName(), GetName().CStr(),
                video_track_needed&& audio_track_needed == true ? "video and audio" :
                video_track_needed == true ? "video" : "audio", item->url.CStr());
            ocst::Orchestrator::GetInstance()->UnmirrorStream(stream_tap);
            return nullptr;
        }

        if (UpdateStream() == false)
        {
            logte("%s/%s: Failed to update stream", GetApplicationName(), GetName().CStr());
            ocst::Orchestrator::GetInstance()->UnmirrorStream(stream_tap);
            return nullptr;
        }

        stream_tap->Start();

        return stream_tap;
    }

    int ScheduledStream::FindTrackIdByOriginId(int origin_id) const
    {
        auto it = _origin_id_track_id_map.find(origin_id);
        if (it == _origin_id_track_id_map.end())
        {
            return -1;
        }

        return it->second;
    }
}