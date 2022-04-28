//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2022 AirenSoft. All rights reserved.
//
//==============================================================================

#include "base/publisher/application.h"
#include "base/publisher/stream.h"

#include "llhls_stream.h"
#include "llhls_private.h"

std::shared_ptr<LLHlsStream> LLHlsStream::Create(const std::shared_ptr<pub::Application> application, const info::Stream &info)
{
	auto stream = std::make_shared<LLHlsStream>(application, info);
	return stream;
}

LLHlsStream::LLHlsStream(const std::shared_ptr<pub::Application> application, const info::Stream &info)
	: Stream(application, info)
{
}

LLHlsStream::~LLHlsStream()
{
	logtd("LLHlsStream(%s/%s) has been terminated finally", GetApplicationName(), GetName().CStr());
}

bool LLHlsStream::Start()
{
	if (GetState() != State::CREATED)
	{
		return false;
	}

	logtd("LLHlsStream(%ld) has been started", GetId());
	
	bmff::FMP4Storage::Config storage_config;
	storage_config.max_segments = 5;
	storage_config.segment_duration_ms = 5000;

	bmff::FMP4Packager::Config packager_config;
	packager_config.chunk_duration_ms = 1000;

	for (const auto &[id, track] : _tracks)
	{
		if ( (track->GetCodecId() == cmn::MediaCodecId::H264) || 
			 (track->GetCodecId() == cmn::MediaCodecId::Aac) )
		{
			if (AddPackager(track, packager_config, storage_config) == false)
			{
				logte("LLHlsStream(%s/%s) - Failed to add packager for track(%ld)", GetApplication()->GetName().CStr(), GetName().CStr(), track->GetId());
				return false;
			}
		}
		else 
		{
			logti("LLHlsStream(%s/%s) - Ignore unsupported codec(%s)", GetApplication()->GetName().CStr(), GetName().CStr(), StringFromMediaCodecId(track->GetCodecId()).CStr());
			continue;
		}
	}

	return Stream::Start();
}

bool LLHlsStream::Stop()
{
	logtd("LLHlsStream(%u) has been stopped", GetId());

	// clear all packagers
	std::lock_guard<std::shared_mutex> lock(_packager_map_lock);
	_packager_map.clear();

	// clear all storages
	std::lock_guard<std::shared_mutex> lock2(_storage_map_lock);
	_storage_map.clear();

	return Stream::Stop();
}

void LLHlsStream::SendVideoFrame(const std::shared_ptr<MediaPacket> &media_packet)
{
	if (GetState() != State::STARTED)
	{
		return;
	}

	AppendMediaPacket(media_packet);
}

void LLHlsStream::SendAudioFrame(const std::shared_ptr<MediaPacket> &media_packet)
{
	if (GetState() != State::STARTED)
	{
		return;
	}

	AppendMediaPacket(media_packet);
}

bool LLHlsStream::AppendMediaPacket(const std::shared_ptr<MediaPacket> &media_packet)
{
	auto track = GetTrack(media_packet->GetTrackId());
	if (track == nullptr)
	{
		logtw("Could not find track. id: %d", media_packet->GetTrackId());
		return false;;
	}

	if ( (track->GetCodecId() == cmn::MediaCodecId::H264) || 
			 (track->GetCodecId() == cmn::MediaCodecId::Aac) )
	{
		// Get Packager
		auto packager = GetPackager(track->GetId());
		if (packager == nullptr)
		{
			logtw("Could not find packager. track id: %d", track->GetId());
			return false;
		}

		logtd("AppendSample : track(%d) length(%d)", media_packet->GetTrackId(), media_packet->GetDataLength());

		packager->AppendSample(media_packet);
	}

	return true;
}

// Create and Get fMP4 packager with track info, storage and packager_config
bool LLHlsStream::AddPackager(const std::shared_ptr<const MediaTrack> &track, const bmff::FMP4Packager::Config &packager_config, const bmff::FMP4Storage::Config &storage_config)
{
	// Create Storage
	auto storage = std::make_shared<bmff::FMP4Storage>(track, storage_config);

	// Create fMP4 Packager
	auto packager = std::make_shared<bmff::FMP4Packager>(storage, track, packager_config);

	// Create Initialization Segment
	if (packager->CreateInitializationSegment() == false)
	{
		logtc("LLHlsStream::AddPackager() - Failed to create initialization segment");
		return false;
	}
	
	// lock
	std::lock_guard<std::shared_mutex> storage_lock(_storage_map_lock);
	std::lock_guard<std::shared_mutex> packager_lock(_packager_map_lock);

	// Add to map
	_storage_map.emplace(track->GetId(), storage);
	_packager_map.emplace(track->GetId(), packager);

	return true;
}

// Get storage with the track id
std::shared_ptr<bmff::FMP4Storage> LLHlsStream::GetStorage(const int32_t &track_id)
{
	std::shared_lock<std::shared_mutex> lock(_storage_map_lock);
	auto it = _storage_map.find(track_id);
	if (it == _storage_map.end())
	{
		return nullptr;
	}

	return it->second;
}

// Get fMP4 packager with the track id
std::shared_ptr<bmff::FMP4Packager> LLHlsStream::GetPackager(const int32_t &track_id)
{
	std::shared_lock<std::shared_mutex> lock(_packager_map_lock);
	auto it = _packager_map.find(track_id);
	if (it == _packager_map.end())
	{
		return nullptr;
	}

	return it->second;
}