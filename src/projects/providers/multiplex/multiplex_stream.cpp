//==============================================================================
//
//  MultiplexStream
//
//  Created by Getroot
//  Copyright (c) 2023 AirenSoft. All rights reserved.
//
//==============================================================================

#include "multiplex_stream.h"
#include "multiplex_private.h"

#include <base/provider/application.h>

namespace pvd
{
    // Implementation of MultiplexStream
    std::shared_ptr<MultiplexStream> MultiplexStream::Create(const std::shared_ptr<Application> &application, const info::Stream &stream_info, const std::shared_ptr<MultiplexProfile> &multiplex_profile)
    {
        auto stream = std::make_shared<pvd::MultiplexStream>(application, stream_info, multiplex_profile);
        return stream;
    }

    MultiplexStream::MultiplexStream(const std::shared_ptr<Application> &application, const info::Stream &info, const std::shared_ptr<MultiplexProfile> &multiplex_profile)
        : Stream(application, info), _multiplex_profile(multiplex_profile)
    {
    }

    MultiplexStream::~MultiplexStream()
    {
        Stop();
    }

    bool MultiplexStream::Start()
    {
        // Create Worker
        _worker_thread_running = true;
        _worker_thread = std::thread(&MultiplexStream::WorkerThread, this);
        pthread_setname_np(_worker_thread.native_handle(), "Multiplex");

        return Stream::Start();
    }

    bool MultiplexStream::Stop()
    {
        if (_worker_thread_running == false)
        {
            return true;
        }

        _worker_thread_running = false;

        if (_worker_thread.joinable())
        {
            _worker_thread.join();
        }

        return Stream::Stop();
    }

    bool MultiplexStream::Terminate()
    {
        return Stream::Terminate();
    }

    void MultiplexStream::WorkerThread()
    {
        while (_worker_thread_running)
        {
            if (PullSourceStreams() == false)
            {
                logte("Scheduled Channel : %s/%s: Failed to pull source streams", GetApplicationName(), GetName().CStr());
                // sleep and retry
                std::this_thread::sleep_for(std::chrono::seconds(1));
                continue;
            }

            break;
        }

        while (_worker_thread_running)
        {
            // Get Streams and Push
            auto source_streams = _multiplex_profile->GetSourceStreams();
            for (auto &source_stream : source_streams)
            {
                auto stream_tap = source_stream->GetStreamTap();
                if (stream_tap == nullptr)
                {
                    continue;
                }

                auto media_packet = stream_tap->Pop(0);
                if (media_packet == nullptr)
                {
                    continue;
                }

                auto source_track_id = MakeSourceTrackIdUnique(stream_tap->GetId(), media_packet->GetTrackId());
                auto new_track_id = GetNewTrackId(source_track_id);
                if (new_track_id == 0)
                {
                    continue;
                }

                media_packet->SetTrackId(new_track_id);

                SendFrame(media_packet);
            }
        }
    }

    uint64_t MultiplexStream::MakeSourceTrackIdUnique(uint32_t tap_id, uint32_t track_id) const
    {
        return (static_cast<uint64_t>(tap_id) << 32) | static_cast<uint64_t>(track_id);
    }

    uint32_t MultiplexStream::GetNewTrackId(uint64_t source_track_id) const
    {
        auto it = _source_track_id_to_new_id_map.find(source_track_id);
        if (it == _source_track_id_to_new_id_map.end())
        {
            return 0;
        }

        return it->second;
    }

    bool MultiplexStream::PullSourceStreams()
    {
        auto source_streams = _multiplex_profile->GetSourceStreams();
        for (auto &source_stream : source_streams)
        {
            auto stream_tap = source_stream->GetStreamTap();
            if (stream_tap == nullptr)
            {
                continue;
            }

            if (stream_tap->GetState() != MediaRouterStreamTap::State::Tapped)
            {
                auto stream_url = source_stream->GetUrl();
                auto vhost_app_name = info::VHostAppName(stream_url->Host(), stream_url->App());

                if (ocst::Orchestrator::GetInstance()->CheckIfStreamExist(vhost_app_name, stream_url->Stream()) == false)
                {
                    logtw("Scheduled Channel : %s/%s: Failed to find stream %s", GetApplicationName(), GetName().CStr(), stream_url->Stream().CStr());
                    return false;
                }

                auto result = ocst::Orchestrator::GetInstance()->MirrorStream(stream_tap, vhost_app_name, stream_url->Stream(), MediaRouterInterface::MirrorPosition::Outbound);

                if (result != CommonErrorCode::SUCCESS)
                {
                    logte("Scheduled Channel : %s/%s: Failed to mirror stream %s (err : %d)", GetApplicationName(), GetName().CStr(), source_stream->GetUrlStr().CStr(), static_cast<int>(result));
                    return false;
                }
            }
        }

        // Make tracks
        for (auto &source_stream : source_streams)
        {
            auto stream_tap = source_stream->GetStreamTap();
            if (stream_tap == nullptr)
            {
                continue;
            }

            auto stream_info = stream_tap->GetStreamInfo();
            if (stream_info == nullptr)
            {
                continue;
            }

            auto tracks = stream_info->GetTracks();
            for (auto &[source_track_id, source_track] : tracks)
            {
                auto source_track_name = source_track->GetVariantName();
                ov::String new_track_name;

                if (source_stream->GetNewTrackName(source_track_name, new_track_name) == false)
                {
                    continue;
                }

                auto new_track = source_track->Clone();
                new_track->SetId(IssueUniqueTrackId());
                new_track->SetVariantName(new_track_name);

                AddTrack(new_track);
                _source_track_id_to_new_id_map.emplace(MakeSourceTrackIdUnique(stream_tap->GetId(), source_track_id), new_track->GetId());

                logti("Multiplex Stream : %s/%s: Added track %s from %s/%s (%d)", GetApplicationName(), GetName().CStr(), new_track_name.CStr(), source_stream->GetUrlStr().CStr(), source_track_name.CStr(), source_track_id);
            }
        }

        // Make Playlist
        auto playlists = _multiplex_profile->GetPlaylists();
        for (auto &playlist : playlists)
        {
            AddPlaylist(playlist);
        }

        // Publish stream
        if (GetApplication()->AddStream(GetSharedPtr()) == false)
        {
            logte("Multiplex Channel : %s/%s: Failed to publish stream", GetApplicationName(), GetName().CStr());
            return false;
        }

        // Start all stream taps
        for (auto &source_stream : source_streams)
        {
            auto stream_tap = source_stream->GetStreamTap();
            if (stream_tap == nullptr)
            {
                continue;
            }

            stream_tap->Start();
        }

        return true;
    }
}