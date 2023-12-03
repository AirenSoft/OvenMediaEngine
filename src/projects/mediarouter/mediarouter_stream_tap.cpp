//==============================================================================
//
//  MediaRouterStreamTap
//
//  Created by Getroot
//  Copyright (c) 2023 AirenSoft. All rights reserved.
//
//==============================================================================

#include "mediarouter_stream_tap.h"

std::shared_ptr<MediaRouterStreamTap> MediaRouterStreamTap::Create(size_t buffer_size)
{
    return std::make_shared<MediaRouterStreamTap>(buffer_size);
}

MediaRouterStreamTap::MediaRouterStreamTap(size_t buffer_size)
    : _buffer("MediaRouterStreamTap", buffer_size)
{
}

MediaRouterStreamTap::~MediaRouterStreamTap()
{
    Destroy();
}

MediaRouterStreamTap::State MediaRouterStreamTap::GetState() const
{
    return _state;
}

std::shared_ptr<MediaPacket> MediaRouterStreamTap::Pop(int timeout_in_msec)
{
    if (_state != State::Tapped && _buffer.IsEmpty())
    {
        return nullptr;
    }

    auto object = _buffer.Dequeue(timeout_in_msec);
    if (object.has_value())
    {
        return object.value();
    }

    return nullptr;
}

std::shared_ptr<info::Stream> MediaRouterStreamTap::GetStreamInfo() const
{
    return _tapped_stream_info;
}

void MediaRouterStreamTap::SetStreamInfo(const std::shared_ptr<info::Stream> &stream_info)
{
    _tapped_stream_info = stream_info;
}

void MediaRouterStreamTap::Destroy()
{
    _is_destroy_requested = true;
}

bool MediaRouterStreamTap::Push(const std::shared_ptr<MediaPacket> &media_packet)
{
    if (_state != State::Tapped)
    {
        return false;
    }

    _buffer.Enqueue(media_packet);

    return true;
}

void MediaRouterStreamTap::SetState(State state)
{
    _state = state;
}
