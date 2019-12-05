#pragma once

#include <base/application/media_track.h>
#include <base/application/media_extradata.h>

#include <unordered_map>
#include <string>

struct RtspMediaInfo
{
    std::unordered_map<uint8_t, MediaTrack> payloads_;
    std::unordered_map<std::string, uint8_t> tracks_;
};