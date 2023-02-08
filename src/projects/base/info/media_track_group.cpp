//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2023 AirenSoft. All rights reserved.
//
//==============================================================================
#include "media_track_group.h"

MediaTrackGroup::MediaTrackGroup(const ov::String &name)
{
	_name = name;
}

MediaTrackGroup::~MediaTrackGroup()
{
}

ov::String MediaTrackGroup::GetName() const
{
	return _name;
}

bool MediaTrackGroup::AddTrack(const std::shared_ptr<MediaTrack> &track)
{
	if (track == nullptr)
	{
		return false;
	}

	_tracks.push_back(track);

	return true;
}

size_t MediaTrackGroup::GetTrackCount() const
{
	return _tracks.size();
}

std::shared_ptr<MediaTrack> MediaTrackGroup::GetFirstTrack() const
{
	return GetTrack(0);
}

std::shared_ptr<MediaTrack> MediaTrackGroup::GetTrack(uint32_t order) const
{
	if (order >= _tracks.size())
	{
		return nullptr;
	}

	return _tracks[order];
}

const std::vector<std::shared_ptr<MediaTrack>> &MediaTrackGroup::GetTracks() const
{
	return _tracks;
}