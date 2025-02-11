//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2024 AirenSoft. All rights reserved.
//
//==============================================================================
#include "mpegts_packager.h"
#include "mpegts_private.h"

namespace mpegts
{
    Packager::Packager(const ov::String &packager_id, const Config &config)
    {
        _packager_id = packager_id;
        _config = config;
    }

    Packager::~Packager()
    {
		if (_config.dvr_storage_path.IsEmpty() == false)
		{
			auto dvr_path = GetDvrStoragePath();
			// Remove directory
			if (ov::DeleteDirectories(dvr_path))
			{
				logti("Deleted temp directory for HLS DVR : %s", dvr_path.CStr());
			}
		}
    }

    const Packager::Config &Packager::GetConfig() const
    {
        return _config;
    }

	bool Packager::AddSink(const std::shared_ptr<PackagerSink> &sink)
	{
		if (sink == nullptr)
		{
			logte("sink is nullptr");
			return false;
		}

		_sinks.push_back(sink);
		return true;
	}

    uint64_t Packager::GetNextSegmentId()
    {
        return _last_segment_id ++;
    }

    std::shared_ptr<ov::Data> Packager::MergeTsPacketData(const std::vector<std::shared_ptr<mpegts::Packet>> &ts_packets)
    {
        auto data = std::make_shared<ov::Data>(SEGMENT_BUFFER_SIZE);

        for (const auto &ts_packet : ts_packets)
        {
            data->Append(ts_packet->GetData());
        }

        return data;
    }
    
    std::shared_ptr<const MediaTrack> Packager::GetMediaTrack(uint32_t track_id) const
    {
        auto it = _media_tracks.find(track_id);
        if (it == _media_tracks.end())
        {
            return nullptr;
        }

        return it->second;
    }

    std::shared_ptr<SampleBuffer> Packager::GetSampleBuffer(uint32_t track_id) const
    {
        auto it = _sample_buffers.find(track_id);
        if (it == _sample_buffers.end())
        {
            return nullptr;
        }

        return it->second;
    }

    void Packager::OnPsi(const std::vector<std::shared_ptr<const MediaTrack>> &tracks, const std::vector<std::shared_ptr<mpegts::Packet>> &psi_packets)
    {
        logtd("OnPsi %u tracks", tracks.size());

        for (const auto &track : tracks)
        {   
            if (track->GetMediaType() != cmn::MediaType::Video && track->GetMediaType() != cmn::MediaType::Audio && track->GetMediaType() != cmn::MediaType::Data)
            {
                logte("Unsupported media type (%s) in Mpeg-2 TS Packager", StringFromMediaType(track->GetMediaType()).CStr());
                continue;
            }

            _media_tracks.emplace(track->GetId(), track);

            if (_main_track_id == UINT32_MAX)
            {
                _main_track_id = track->GetId();
            }

            if (track->GetMediaType() == cmn::MediaType::Video)
            {
                _main_track_id = track->GetId();
            }

            // Create SampleBuffer
            _sample_buffers.emplace(track->GetId(), std::make_shared<SampleBuffer>(track));
        }

        _psi_packets = psi_packets;
        _psi_packet_data = MergeTsPacketData(psi_packets);
    }

	bool Packager::InsertMarker(uint32_t data_track_id, const Marker &marker)
	{
		std::lock_guard<std::shared_mutex> lock(_markers_guard);

		auto data_track = GetMediaTrack(data_track_id);
		if (data_track == nullptr)
		{
			logte("Data track is not found for track_id %u", data_track_id);
			return false;
		}

		// mpeg-2 ts time base is 90khz
		int64_t timestamp = (static_cast<double>(marker.timestamp) / data_track->GetTimeBase().GetTimescale() * TIMEBASE_DBL);
	
		Marker converted_marker = marker;
		converted_marker.timestamp = timestamp;

		// if there is CUE-IN event in the marker list, remove it
		if (marker.tag.UpperCaseString() == "CUEEVENT-IN")
		{
			// first item is CUE-IN event in the marker list, remove it
			if (_markers.empty() == false)
			{
				auto it = _markers.begin();
				if (it->second.tag.UpperCaseString() == "CUEEVENT-IN")
				{
					logti("CUE-IN event is already in the marker list, remove it");
					_markers.erase(it);
				}
			}
			else if (_last_removed_marker.tag.UpperCaseString() == "CUEEVENT-IN" || _last_removed_marker.tag.IsEmpty())
			{
				// if it was already applied, do not insert
				logtw("CUE-IN event needs CUE-OUT event before it, cannot insert CUE-IN event");
				return false;
			}
		}
		else if (marker.tag.UpperCaseString() == "CUEEVENT-OUT")
		{
			// if there is CUE-OUT event in the marker list, remove it
			if (_markers.empty() == false)
			{
				auto it = _markers.begin();
				auto first_marker = it->second;
				if (first_marker.tag.UpperCaseString() == "CUEEVENT-OUT")
				{
					// Cannot insert CUE-OUT event if there is already CUE-OUT event
					logtw("Cannot insert CUE-OUT event if there is already CUE-OUT event in the marker list");
					return false;
				}
				else if (first_marker.timestamp >= timestamp)
				{
					// Cannot insert CUE-OUT event before the CUE-IN event
					logtw("Cannot insert CUE-OUT event before the CUE-IN event");
					return false;
				}
			}
		}

		_markers.emplace(timestamp, converted_marker);

		return true;
	}

	bool Packager::HasMarker() const
	{
		std::shared_lock<std::shared_mutex> lock(_markers_guard);
		return _markers.empty() == false;
	}

	const Marker Packager::GetFirstMarker() const
	{
		std::shared_lock<std::shared_mutex> lock(_markers_guard);
		if (_markers.empty() == true)
		{
			return Marker();
		}

		return _markers.begin()->second;
	}

	bool Packager::RemoveMarker(int64_t timestamp)
	{
		std::lock_guard<std::shared_mutex> lock(_markers_guard);

		auto it = _markers.find(timestamp);
		if (it == _markers.end())
		{
			return false;
		}

		// It means that the last removed marker was applied
		_last_removed_marker = it->second;

		_markers.erase(it);
		return true;
	}

    void Packager::OnFrame(const std::shared_ptr<const MediaPacket> &media_packet, const std::vector<std::shared_ptr<mpegts::Packet>> &pes_packets)
    {
       //logtd("OnFrame track_id %u", media_packet->GetTrackId());

        auto track_id = media_packet->GetTrackId();
        auto track = GetMediaTrack(track_id);

        auto sample_buffer = GetSampleBuffer(track_id);
        if (track == nullptr || sample_buffer == nullptr)
        {
            logte("SampleBuffer is not found for track_id %u", track_id);
            return;
        }

		auto sample = mpegts::Sample(media_packet, MergeTsPacketData(pes_packets), track->GetTimeBase().GetTimescale());

		if (track_id == _main_track_id)
		{
			if (_force_make_boundary == false && HasMarker() == true)
			{
				auto marker = GetFirstMarker();
				if (marker.timestamp >= sample._dts && marker.timestamp < sample._dts + sample._duration)
				{
					logti("Stream(%s) Track(%u) has a marker at %lld (%lld - %lld), force to create a new boundary", _config.stream_id_meta.CStr(), track_id, marker.timestamp, sample._dts, sample._dts + sample._duration);

					_force_make_boundary = true;
				}
			}

			if ((sample_buffer->GetCurrentDurationMs() >= _config.target_duration_ms) || _force_make_boundary == true)
			{
				if (media_packet->GetMediaType() == cmn::MediaType::Video && media_packet->IsKeyFrame())
				{
					sample_buffer->MarkSegmentBoundary();
					_force_make_boundary = false;
				}
				else if (media_packet->GetMediaType() == cmn::MediaType::Audio)
				{
					sample_buffer->MarkSegmentBoundary();
					_force_make_boundary = false;
				}
			}
		}

        sample_buffer->AddSample(sample);

        CreateSegmentIfReady();
    }

	void Packager::Flush()
	{
		auto sample_buffer = GetSampleBuffer(_main_track_id);
		if (sample_buffer == nullptr)
		{
			return;
		}

		sample_buffer->MarkSegmentBoundary();

		CreateSegmentIfReady(true);
	}

	std::shared_ptr<Segment> Packager::GetSegment(uint64_t segment_id) const
	{
		{
			std::shared_lock<std::shared_mutex> lock(_segments_guard);
			auto it = _segments.find(segment_id);
			if (it != _segments.end())
			{
				return it->second;
			}
		}

		{
			std::shared_lock<std::shared_mutex> lock(_file_stored_segments_guard);
			auto it = _file_stored_segments.find(segment_id);
			if (it != _file_stored_segments.end())
			{
				return it->second;
			}
		}

		{
			std::shared_lock<std::shared_mutex> lock(_retained_segments_guard);
			auto it = _retained_segments.find(segment_id);
			if (it != _retained_segments.end())
			{
				return it->second;
			}
		}

		return nullptr;
	}

	std::shared_ptr<const ov::Data> Packager::GetSegmentData(uint64_t segment_id) const
	{
		auto segment = GetSegment(segment_id);
		if (segment == nullptr)
		{
			return nullptr;
		}

		return segment->GetData();
	}

    void Packager::CreateSegmentIfReady(bool force_create)
    {
        // Check if the main track has a segment boundary
        auto main_sample_buffer = GetSampleBuffer(_main_track_id);
        if (main_sample_buffer == nullptr)
        {
            logtc("Stream(%s) SampleBuffer is not found for main track_id %u", _config.stream_id_meta.CStr(), _main_track_id);
            return;
        }

		// If the segment duration is too long (twice the target duration), a new segment is forcibly created.
		if (force_create == false && main_sample_buffer->GetTotalAvailableDurationMs() >= _config.target_duration_ms * 2)
		{
			logtw("Stream(%s) Main Track(%u) has too long duration (%.3f ms, twice the target duration %u ms), force to create a new segment", _config.stream_id_meta.CStr(), _main_track_id, main_sample_buffer->GetTotalAvailableDurationMs(), _config.target_duration_ms);
			
			if (main_sample_buffer->HasSegmentBoundary() == false)
			{
				Flush();
			}
			else
			{
				force_create = true;
			}
		}

        if (main_sample_buffer->HasSegmentBoundary() == false)
        {
			return;
        }

		auto main_segment_duration = main_sample_buffer->GetDurationUntilSegmentBoundary();
		auto total_main_segment_duration = main_sample_buffer->GetTotalConsumedDuration() + main_segment_duration;
        auto main_segment_duration_ms = main_sample_buffer->GetDurationUntilSegmentBoundaryMs();

		std::vector<Marker> markers;
		if (HasMarker() == true)
		{
			int64_t main_segment_base_timestamp = main_sample_buffer->GetSample()._dts;
			int64_t main_segment_end_timestamp = main_segment_base_timestamp + main_segment_duration;

			logtd("Stream(%s) Main Track(%u) main_segment_base_timestamp(%lld) main_segment_duration(%lld) main_segment_duration_ms(%f) main_segment_end_timestamp(%lld)", _config.stream_id_meta.CStr(), _main_track_id, main_segment_base_timestamp, main_segment_duration, main_segment_duration_ms, main_segment_end_timestamp);

			while (HasMarker())
			{
				auto marker = GetFirstMarker();
				if (marker.timestamp < main_segment_base_timestamp)
				{
					logte("Stream(%s) Main Track(%u) has a marker at %lld, but it is before the current segment %lld", _config.stream_id_meta.CStr(), _main_track_id, marker.timestamp, main_segment_base_timestamp);
					RemoveMarker(marker.timestamp);
				}
				else
				{
					break;
				}
			}

			// All Marker can be removed so check again
			while (HasMarker())
			{
				auto marker = GetFirstMarker();
				if (marker.timestamp >= main_segment_base_timestamp && marker.timestamp < main_segment_end_timestamp)
				{
					logti("Stream(%s) Main Track(%u) Segment(%lld) has a marker at %lld, tag : %s", _config.stream_id_meta.CStr(), _main_track_id, main_segment_base_timestamp, marker.timestamp, marker.tag.CStr());

					markers.push_back(marker);

					RemoveMarker(marker.timestamp);
					force_create = true;
				}
				else
				{
					break;
				}
			}
		}

		if (force_create == false)
		{
			// Check if all tracks have enough samples
			for (const auto &[track_id, sample_buffer] : _sample_buffers)
			{
				if (track_id == _main_track_id)
				{
					continue;
				}

				if (GetMediaTrack(track_id)->GetMediaType() == cmn::MediaType::Data)
				{
					continue;
				}
				
				auto total_sample_segment_duration = sample_buffer->GetTotalConsumedDuration() + sample_buffer->GetCurrentDuration();

				// if video segment is 6000, audio segment is at least 6000*0.97(=5820), it is normal case, wait for more samples
				if (static_cast<double>(total_sample_segment_duration) < static_cast<double>(total_main_segment_duration) * 0.97)
				{
					// Too much difference between the main track and the track, it means that the track may have a problem.
					// For example, there may be cases where audio stops coming in at all at some point.
					if (total_sample_segment_duration * 2.0 < total_main_segment_duration)
					{
						logtw("Stream(%s) Track(%u) sample duration (%lld) is less than half of the main (%lld), forcing segment generation.", _config.stream_id_meta.CStr(), track_id, total_sample_segment_duration, total_main_segment_duration);
					}
					else
					{
						return;
					}
				}
			}
		}

        // Create a segment
        auto first_sample = main_sample_buffer->GetSample();
        if (first_sample.media_packet == nullptr)
        {
            logte("First sample is not found for main track_id %u", _main_track_id);
            return;
        }

        auto segment = std::make_shared<Segment>(GetNextSegmentId(), first_sample._dts, main_segment_duration_ms);
		if (markers.empty() == false)
		{
			segment->SetMarkers(markers);
		}

        // Add PSI packets
        segment->AddPacketData(_psi_packet_data);

        // Add Samples
        auto main_samples = main_sample_buffer->PopSamplesUntilSegmentBoundary();

        // Align samples with the main samples
        for (const auto &main_sample : main_samples)
        {
            segment->AddPacketData(main_sample.ts_packet_data);
            auto main_timestamp = main_sample._dts;

            for (const auto &[track_id, sample_buffer] : _sample_buffers)
            {
                if (track_id == _main_track_id)
                {
                    continue;
                }

                if (sample_buffer->IsEmpty())
                {
                    continue;
                }

                // Align samples with the main track
                while (true)
                {
                    auto sample = sample_buffer->GetSample();
                    if (sample.media_packet == nullptr)
                    {
                        sample_buffer->PopSample();
                        break;
                    }

                    if (sample._dts <= main_timestamp)
                    {
                        segment->AddPacketData(sample.ts_packet_data);
                        sample_buffer->PopSample();
                    }
                    else
                    {
                        break;
                    }
                }
            }
        }

        // Add remaining samples
        std::map<uint32_t, bool> completed_tracks;
        completed_tracks[_main_track_id] = true;

        while (completed_tracks.size() < _sample_buffers.size())
        {
            for (const auto &[track_id, sample_buffer] : _sample_buffers)
            {
                if (completed_tracks.find(track_id) != completed_tracks.end())
                {
                    continue;
                }

				if (GetMediaTrack(track_id)->GetMediaType() == cmn::MediaType::Data)
				{
					completed_tracks[track_id] = true;
					continue;
				}
                
                if (sample_buffer->IsEmpty())
                {
                    completed_tracks[track_id] = true;
                    continue;
                }

                if (sample_buffer->GetTotalConsumedDuration() >= total_main_segment_duration)
                {
					// Wraparound, if total_main_segment_duration_us is wrapped around, continue to pop samples until the total_consumed_duration_us is wrapped around
					if (sample_buffer->GetTotalConsumedDuration() - total_main_segment_duration < UINT64_MAX / 2)
					{
                    	completed_tracks[track_id] = true;
                    	continue;
					}
                }

                auto sample = sample_buffer->PopSample();
                if (sample.media_packet == nullptr)
                {
                    // Error
                    completed_tracks[track_id] = true;
                    break;
                }

                segment->AddPacketData(sample.ts_packet_data);
            }
        }

        AddSegment(segment);
    }

    void Packager::AddSegment(const std::shared_ptr<Segment> &segment)
    {
#if 0
        logti("AddSegment segment_id %u", segment->GetId());
        auto file_name = ov::String::FormatString("segment_%u.ts", segment->GetId());
        ov::DumpToFile(file_name.CStr(), segment->GetData());
#endif 

		AddSegmentToBuffer(segment);
		BroadcastSegmentCreated(segment);

		if (GetBufferedSegmentCount() > _config.max_segment_count)
		{
			auto segment = GetOldestSegmentFromBuffer();
			if (_config.dvr_window_ms > 0)
			{
				SaveSegmentToFile(segment);
			}
			else if (_config.segment_retention_count > 0)
			{
				SaveSegmentToRetentionBuffer(segment);
			}
			else
			{
				BroadcastSegmentDeleted(segment);
			}

			RemoveSegmentFromBuffer(segment);
		}
    }

	void Packager::AddSegmentToBuffer(const std::shared_ptr<Segment> &segment)
	{
		std::lock_guard<std::shared_mutex> lock(_segments_guard);
		_segments.emplace(segment->GetId(), segment);
		_total_segments_duration_ms += segment->GetDurationMs();
	}
	
	size_t Packager::GetBufferedSegmentCount() const
	{
		std::shared_lock<std::shared_mutex> lock(_segments_guard);
		return _segments.size();
	}

	std::shared_ptr<Segment> Packager::GetOldestSegmentFromBuffer() const
	{
		std::shared_lock<std::shared_mutex> lock(_segments_guard);
		if (_segments.empty())
		{
			return nullptr;
		}

		auto it = _segments.begin();
		return it->second;
	}

	void Packager::RemoveSegmentFromBuffer(const std::shared_ptr<Segment> &segment)
	{
		std::lock_guard<std::shared_mutex> lock(_segments_guard);
		auto it = _segments.find(segment->GetId());
		if (it != _segments.end())
		{
			_segments.erase(it);
			_total_segments_duration_ms -= segment->GetDurationMs();
		}
	}

	void Packager::SaveSegmentToFile(const std::shared_ptr<Segment> &segment)
	{
		auto file_path = GetSegmentFilePath(segment->GetId());
		auto dir = GetDvrStoragePath();
		
		// Create directory
		if (ov::IsDirExist(dir) == false)
		{
			logti("Try to create directory for HLS DVR: %s", dir.CStr());
			if (ov::CreateDirectories(dir) == false)
			{
				logte("Could not create directory for DVR: %s", dir.CStr());
				return;
			}
		}

		// Save to file
		if (ov::DumpToFile(file_path.CStr(), segment->GetData()) == nullptr)
		{
			logte("Failed to save segment to file: %s", file_path.CStr());
			return;
		}

		logtd("Saved segment to file: %s", file_path.CStr());

		// Add segment info
		{
			std::lock_guard<std::shared_mutex> lock(_file_stored_segments_guard);
			// Remove data from segment, it has been saved in a file
			segment->ResetData();
			segment->SetFilePath(file_path);
			_total_file_stored_segments_duration_ms += segment->GetDurationMs();
			_file_stored_segments.emplace(segment->GetId(), segment);
		}

		// Delete old segments from stored list and file
		while (GetTotalFileStoredSegmentsDurationMs() > _config.dvr_window_ms)
		{
			auto oldest_segment = GetOldestSegmentFromFile();
			
			if (_config.segment_retention_count > 0)
			{
				SaveSegmentToRetentionBuffer(oldest_segment);
			}
			else
			{
				BroadcastSegmentDeleted(oldest_segment);
				DeleteSegmentFile(oldest_segment);
			}

			DeleteSegmentFromFileStoredList(oldest_segment);
		}
	}

	double Packager::GetTotalFileStoredSegmentsDurationMs() const
	{
		std::shared_lock<std::shared_mutex> lock(_file_stored_segments_guard);
		return _total_file_stored_segments_duration_ms;
	}

	std::shared_ptr<Segment> Packager::GetOldestSegmentFromFile() const
	{
		std::shared_lock<std::shared_mutex> lock(_file_stored_segments_guard);
		if (_file_stored_segments.empty())
		{
			return nullptr;
		}

		auto it = _file_stored_segments.begin();
		return it->second;
	}

	void Packager::DeleteSegmentFromFileStoredList(const std::shared_ptr<Segment> &segment)
	{
		std::lock_guard<std::shared_mutex> lock(_file_stored_segments_guard);
		auto it = _file_stored_segments.find(segment->GetId());
		if (it != _file_stored_segments.end())
		{
			_total_file_stored_segments_duration_ms -= segment->GetDurationMs();
			_file_stored_segments.erase(it);
		}
	}

	void Packager::DeleteSegmentFile(const std::shared_ptr<Segment> &segment)
	{
		auto file_path = segment->GetFilePath();
		if (ov::DeleteFile(file_path) == false)
		{
			logte("Failed to delete segment file: %s", file_path.CStr());
			return;
		}

		logtd("Deleted segment file: %s", file_path.CStr());
	}

	void Packager::SaveSegmentToRetentionBuffer(const std::shared_ptr<Segment> &segment)
	{
		{
			std::lock_guard<std::shared_mutex> lock(_retained_segments_guard);
			_retained_segments.emplace(segment->GetId(), segment);

			logtd("Saved segment to retention buffer: %u", segment->GetId());

			// When a segment queues the retention buffer, it is notified that the segment has been deleted. 
			// And the retention buffer stores this as much as count. 
			// This makes it possible to respond even to late requests.
			BroadcastSegmentDeleted(segment);
		}

		// Delete old segments from retention buffer
		if (GetReteinedSegmentCount() > _config.segment_retention_count)
		{
			auto oldest_segment = GetOldestSegmentFromRetentionBuffer();
			RemoveSegmentFromRetentionBuffer(oldest_segment);

			if (oldest_segment->GetFilePath().IsEmpty() == false)
			{
				DeleteSegmentFile(oldest_segment);
			}
		}
	}

	size_t Packager::GetReteinedSegmentCount() const
	{
		std::shared_lock<std::shared_mutex> lock(_retained_segments_guard);
		return _retained_segments.size();
	}

	std::shared_ptr<Segment> Packager::GetOldestSegmentFromRetentionBuffer() const
	{
		std::shared_lock<std::shared_mutex> lock(_retained_segments_guard);
		if (_retained_segments.empty())
		{
			return nullptr;
		}

		auto it = _retained_segments.begin();
		return it->second;
	}

	void Packager::RemoveSegmentFromRetentionBuffer(const std::shared_ptr<Segment> &segment)
	{
		std::lock_guard<std::shared_mutex> lock(_retained_segments_guard);
		auto it = _retained_segments.find(segment->GetId());
		if (it != _retained_segments.end())
		{
			_retained_segments.erase(it);
		}
	}

	void Packager::BroadcastSegmentCreated(const std::shared_ptr<Segment> &segment)
	{
		for (const auto &sink : _sinks)
		{
			sink->OnSegmentCreated(_packager_id, segment);
		}
	}

	void Packager::BroadcastSegmentDeleted(const std::shared_ptr<Segment> &segment)
	{
		for (const auto &sink : _sinks)
		{
			sink->OnSegmentDeleted(_packager_id, segment);
		}
	}

	ov::String Packager::GetDvrStoragePath() const
	{
		return ov::String::FormatString("%s/%s/%s", _config.dvr_storage_path.CStr(), _config.stream_id_meta.CStr(), _packager_id.CStr());
	}

	ov::String Packager::GetSegmentFilePath(uint64_t segment_id) const
	{
		return ov::String::FormatString("%s/segment_%u_hls.ts", GetDvrStoragePath().CStr(), segment_id);
	}
}