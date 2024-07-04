//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2024 AirenSoft. All rights reserved.
//
//==============================================================================
#include "mpegts_packetizer.h"
#include "mpegts_private.h"

namespace mpegts
{
    Packetizer::Packetizer()
    {
        // default config
        _config.mpegts_packet_size = 188;
    }

    Packetizer::Packetizer(const Config &config)
    {
        _config = config;
    }

    Packetizer::~Packetizer()
    {
    }

    bool Packetizer::AddSink(const std::shared_ptr<PacketizerSink> &sink)
    {
        _sinks.push_back(sink);
        return true;
    }

    bool Packetizer::RemoveSink(const std::shared_ptr<PacketizerSink> &sink)
    {
        auto it = std::find(_sinks.begin(), _sinks.end(), sink);
        if (it != _sinks.end())
        {
            _sinks.erase(it);
        }
        return true;
    }

    bool Packetizer::AddTrack(const std::shared_ptr<const MediaTrack> &media_track)
    {
        if (IsStarted())
        {
            return false;
        }

        auto stream_type = GetElementaryStreamType(media_track->GetCodecId());
        if (stream_type == WellKnownStreamTypes::None)
        {
            logte("%s codec is not supported", cmn::GetCodecIdToString(media_track->GetCodecId()).CStr());
            return false;
        }

        _media_tracks.emplace(media_track->GetId(), media_track);

        if (media_track->GetMediaType() == cmn::MediaType::Video)
        {
            _first_video_frame_received = false;
            _has_video = true;
        }

        return true;
    }

    std::shared_ptr<const MediaTrack> Packetizer::GetMediaTrack(uint32_t track_id) const
    {
        auto it = _media_tracks.find(track_id);
        if (it != _media_tracks.end())
        {
            return it->second;
        }

        return nullptr;
    }

    bool Packetizer::Start()
    {
        if (_started)
        {
            return false;
        }

        _pat_packet = BuildPatPacket();
        if (_pat_packet == nullptr)
        {
            return false;
        }

        _pmt_packet = BuildPmtPacket();
        if (_pmt_packet == nullptr)
        {
            return false;
        }

#if 0
        // Debug
        logtd("PAT : %s", _pat_packet->GetData()->ToHexString().CStr());
        logtd("PMT : %s", _pmt_packet->GetData()->ToHexString().CStr());
#endif

        BroadcastPsi();

        _started = true;
        return true;
    }

    bool Packetizer::IsStarted() const
    {
        return _started;
    }

    bool Packetizer::Stop()
    {
        _started = false;
        return true;
    }
    
    bool Packetizer::AppendFrame(const std::shared_ptr<const MediaPacket> &media_packet)
    {
        if (IsStarted() == false)
        {
            return false;
        }

        if (_has_video == true && _first_video_frame_received == false && media_packet->GetMediaType() == cmn::MediaType::Video)
        {
            if (media_packet->IsKeyFrame() == false)
            {
				logtw("First video frame is not key frame so it has been dropped : %s", media_packet->GetInfoString().CStr());
                return false;
            }

            _first_video_frame_received = true;
        }

        // logtd("AppendFrame track_id %u, media_type %s, pts %lld, dts %lld", media_packet->GetTrackId(), StringFromMediaType(media_packet->GetMediaType()).CStr(), media_packet->GetPts(), media_packet->GetDts());

        auto track = GetMediaTrack(media_packet->GetTrackId());
        if (track == nullptr)
        {
            return false;
        }

        auto pid = GetElementaryPid(media_packet->GetTrackId());
        auto pes = Pes::Build(pid, track, media_packet);
        if (pes == nullptr)
        {
            return false;
        }

        auto continuity_counter = GetNextContinuityCounter(pid);
        bool has_pcr = (pid == _pmt._pcr_pid);
        auto packets = Packet::Build(pes, has_pcr, continuity_counter);
        if (packets.empty())
        {
            return false;
        }

#if 0
        // debug print
        logtd("------------------------------------------------------------------------");
        logtd("Track(%u) / MediaPacket(%u)", media_packet->GetTrackId(), media_packet->GetData()->GetLength());
        for (const auto &packet : packets)
        {
            logtd("%s", packet->ToDebugString().CStr());
        }
#endif

        IncreaseContinuityCounter(pid, static_cast<uint8_t>(packets.size() - 1));

        BroadcastFrame(media_packet, packets);

        return true;
    }

    const Packetizer::Config &Packetizer::GetConfig() const
    {
        return _config;
    }

    std::shared_ptr<mpegts::Packet> Packetizer::BuildPatPacket()
    {
        _pat._program_num = PROGRAM_NUMBER;
        _pat._program_map_pid = PMT_PID;

        auto pat_section = Section::Build(_pat);
        if (pat_section == nullptr)
        {
            return nullptr;
        }

        auto pat_packet = Packet::Build(pat_section, 0);
        if (pat_packet == nullptr)
        {
            return nullptr;
        }

        return pat_packet;
    }

    std::shared_ptr<mpegts::Packet> Packetizer::BuildPmtPacket()
    {
        _pmt._pid = PMT_PID;
    
        for (const auto &track_it : _media_tracks)
        {
            auto track = track_it.second;
            auto es_info = std::make_shared<ESInfo>();

            es_info->_stream_type = static_cast<uint8_t>(GetElementaryStreamType(track->GetCodecId()));
            if (es_info->_stream_type == 0)
            {
                logte("%s codec is not supported", cmn::GetCodecIdToString(track->GetCodecId()).CStr());
                return nullptr;
            }

            auto elementary_pid = GetElementaryPid(track->GetId());
            if (_pmt._pcr_pid == 0x1FFF && track->GetMediaType() == cmn::MediaType::Video)
            {
                _pmt._pcr_pid = elementary_pid;
            }
            
            es_info->_elementary_pid = elementary_pid;
            es_info->_es_info_length = 0; // descriptors length

            _pmt._es_info_list.push_back(es_info);
        }

        if (_pmt._pcr_pid == 0x1FFF && GetFirstElementaryPid() != 0)
        {
            _pmt._pcr_pid = GetFirstElementaryPid();
        }

        auto pmt_section = Section::Build(_pmt);
        if (pmt_section == nullptr)
        {
            return nullptr;
        }

        auto pmt_packet = Packet::Build(pmt_section, 0);
        if (pmt_packet == nullptr)
        {
            return nullptr;
        }

        return pmt_packet;
    }

    uint16_t Packetizer::GetElementaryPid(uint32_t track_id)
    {
        auto it = _pids.find(track_id);
        if (it != _pids.end())
        {
            return it->second;
        }

        // elementary pid starts from 0x101
        auto pid = static_cast<uint16_t>(MIN_ELEMENTARY_PID + _pids.size());
        _pids.emplace(track_id, pid);

        return pid;
    }

    uint16_t Packetizer::GetFirstElementaryPid() const
    {
        if (_pids.empty())
        {
            return 0;
        }

        return _pids.begin()->second;
    }

    WellKnownStreamTypes Packetizer::GetElementaryStreamType(cmn::MediaCodecId codec_id) const
    {
        switch (codec_id)
        {
        case cmn::MediaCodecId::H264:
            return WellKnownStreamTypes::H264;
        case cmn::MediaCodecId::H265:
            return WellKnownStreamTypes::H265;
        case cmn::MediaCodecId::Aac:
            return WellKnownStreamTypes::AAC;
        default:
            return WellKnownStreamTypes::None;
        }

        return WellKnownStreamTypes::None;
    }

    uint8_t Packetizer::GetNextContinuityCounter(uint16_t pid)
    {
        auto it = _continuity_counters.find(pid);
        if (it == _continuity_counters.end())
        {
            _continuity_counters[pid] = 0;
            return 0;
        }

        auto &continuity_counter = it->second;
        continuity_counter = (continuity_counter + 1) % 16;

        return continuity_counter;
    }

    void Packetizer::IncreaseContinuityCounter(uint16_t pid, uint32_t add_count)
    {
        auto it = _continuity_counters.find(pid);
        if (it != _continuity_counters.end())
        {
            uint32_t continuity_counter = it->second;
            it->second = (continuity_counter + add_count) % 16;
        }
    }

    void Packetizer::BroadcastPsi()
    {
        auto tracks = std::vector<std::shared_ptr<const MediaTrack>>();
        for (const auto &track_it : _media_tracks)
        {
            tracks.push_back(track_it.second);
        }

        for (const auto &sink : _sinks)
        {
            sink->OnPsi(tracks, {_pat_packet, _pmt_packet});
        }
    }

    void Packetizer::BroadcastFrame(const std::shared_ptr<const MediaPacket> &media_packet, const std::vector<std::shared_ptr<mpegts::Packet>> &pes_packets)
    {
        for (const auto &sink : _sinks)
        {
            sink->OnFrame(media_packet, pes_packets);
        }
    }
}