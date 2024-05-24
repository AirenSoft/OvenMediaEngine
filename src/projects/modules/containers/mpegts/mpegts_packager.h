//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2024 AirenSoft. All rights reserved.
//
//==============================================================================
#pragma once

#include <base/info/media_track.h>
#include <base/mediarouter/media_buffer.h>

#include "mpegts_packetizer.h"

namespace mpegts
{
    constexpr size_t SEGMENT_BUFFER_SIZE = 2000000;

    class Segment
    {
    public:
        Segment(uint32_t segment_id, uint32_t first_dts, uint32_t duration_ms)
        {
            _segment_id = segment_id;
            _first_dts = first_dts;
            _duration_ms = duration_ms;
        }

        bool AddPacketData(const std::shared_ptr<const ov::Data> &data)
        {
            if (_data == nullptr)
            {
                _data = std::make_shared<ov::Data>(SEGMENT_BUFFER_SIZE);
            }

            _data->Append(data);

			_is_data_in_memory = true;

            return true;
        }

        uint32_t GetId() const
        {
            return _segment_id;
        }

		uint32_t GetNumber() const
		{
			return _segment_id;
		}

		ov::String GetUrl() const
		{
			return _url;
		}

		void SetUrl(const ov::String &url)
		{
			_url = url;
		}

        int64_t GetFirstTimestamp() const
        {
            return _first_dts;
        }

        uint32_t GetDurationMs() const
        {
            return _duration_ms;
        }

		ov::String GetFilePath() const
		{
			return _file_path;
		}

		void SetFilePath(const ov::String &file_path)
		{
			_file_path = file_path;

			_is_data_in_file = true;
		}

		void ResetData()
		{
			_data.reset();
			_data = nullptr;

			_is_data_in_memory = false;
		}

		bool IsDataInMemory() const
		{
			return _is_data_in_memory;
		}

		bool IsDataInFile() const
		{
			return _is_data_in_file;
		}

		std::shared_ptr<const ov::Data> GetData() const
		{
			if (_is_data_in_memory)
			{
				return _data;
			}

			if (_is_data_in_file)
			{
				// Read from file
				auto data = ov::LoadFromFile(_file_path);
				if (data == nullptr)
				{
					loge("MPEG-2 TS", "Segment::GetData - Failed to load data from file(%s)", _file_path.CStr());
				}
				return data;
			}

			return nullptr;
		}

    private:
        uint32_t _segment_id = 0;
        int64_t _first_dts = -1;
        uint32_t _duration_ms = 0;
		ov::String _url;
        
		ov::String _file_path;
        std::shared_ptr<ov::Data> _data;

		bool _is_data_in_memory = false;
		bool _is_data_in_file = false;
    };

    struct Sample
    {
        Sample(const std::shared_ptr<const MediaPacket> &media_packet, const std::shared_ptr<const ov::Data> &ts_packet_data)
        {
            this->media_packet = media_packet;
            this->ts_packet_data = ts_packet_data;
        }

        std::shared_ptr<const MediaPacket> media_packet = nullptr;
        std::shared_ptr<const ov::Data> ts_packet_data = nullptr;
    };

    class SampleBuffer
    {
    public:
        SampleBuffer(const std::shared_ptr<const MediaTrack> &track)
        {
            _track = track;
        }

        std::shared_ptr<const MediaTrack> GetTrack() const
        {
            return _track;
        }

        bool AddSample(const Sample &sample)
        {
            _samples.push(sample);

            auto duration_ms = sample.media_packet->GetDuration() * 1000 / GetTrack()->GetTimeBase().GetTimescale();

            _current_samples_count++;
            _current_samples_duration_ms += duration_ms;

            return true;
        }
        
        uint32_t GetCurrentDurationMs() const
        {
            return _current_samples_duration_ms;
        }

        bool HasSegmentBoundary() const
        {
            return _segment_boundaries.empty() == false;
        }

        // number of boundary 
        size_t GetSegmentBoundaryCount() const
        {
            return _segment_boundaries.size();
        }

        void MarkSegmentBoundary()
        {
            SegmentBoundary boundary;
            boundary.sample_count = _current_samples_count;
            boundary.duration_ms = _current_samples_duration_ms;

            _segment_boundaries.push(boundary);

            _current_samples_count = 0;
            _current_samples_duration_ms = 0;
        }

        uint32_t GetDurationUntilSegmentBoundaryMs() const
        {
            if (HasSegmentBoundary() == false)
            {
                return 0;
            }

            return _segment_boundaries.front().duration_ms;
        }
        
        bool IsEmpty() const
        {
            return _samples.empty();
        }

        const Sample PopSample()
        {
            if (_samples.empty())
            {
                return Sample(nullptr, nullptr);
            }

            auto sample = _samples.front();
            _samples.pop();

            auto sample_duration_ms = sample.media_packet->GetDuration() * 1000 / GetTrack()->GetTimeBase().GetTimescale();

            _current_samples_count--;
            _current_samples_duration_ms -= sample_duration_ms;

            _total_consumed_samples_count++;
            _total_consumed_samples_duration_ms += sample_duration_ms;

            return sample;
        }

        const Sample GetSample() const
        {
            if (_samples.empty())
            {
                return Sample(nullptr, nullptr);
            }

            return _samples.front();
        }

        std::vector<Sample> PopSamplesUntilSegmentBoundary()
        {
            std::vector<Sample> samples;

            SegmentBoundary boundary = _segment_boundaries.front();
            _segment_boundaries.pop();

            //logd("DEBUG", "PopSamplesUntilSegmentBoundary : sample_count %u, duration_ms %u", boundary.sample_count, boundary.duration_ms);

            for (uint32_t i = 0; i < boundary.sample_count; i++)
            {
                if (_samples.empty())
                {
                    // would not happen
                    break;
                }

                auto sample = _samples.front();
                _samples.pop();

                samples.push_back(sample);
            }

            _total_consumed_samples_count += boundary.sample_count;
            _total_consumed_samples_duration_ms += boundary.duration_ms;

            return samples;
        }

        uint32_t GetTotalConsumedDurationMs() const
        {
            return _total_consumed_samples_duration_ms;
        }

    private:
        std::shared_ptr<const MediaTrack> _track;
        std::queue<Sample> _samples;

        struct SegmentBoundary
        {
            uint32_t sample_count = 0;
            uint32_t duration_ms = 0;
        };

        std::queue<SegmentBoundary> _segment_boundaries;

        uint32_t _current_samples_count = 0;
        uint32_t _current_samples_duration_ms = 0;

        uint32_t _total_consumed_samples_count = 0;
        uint32_t _total_consumed_samples_duration_ms = 0;
    };

	class PackagerSink : public ov::EnableSharedFromThis<PackagerSink>
	{
	public:
		virtual void OnSegmentCreated(const ov::String &packager_id, const std::shared_ptr<Segment> &segment) = 0;
		virtual void OnSegmentDeleted(const ov::String &packager_id, const std::shared_ptr<Segment> &segment) = 0;
	};

    // Make mpeg-2 ts container

	// Segments are stored in the following locations:
	// DVR off, Retention 0 	: Buffer
	// DVR off, Retention > 0 	: Buffer --> Retention
	// DVR on, Retention > 0	: Buffer --> DVR(file) --> Retention(file) 
	// DVR on, Retention 0		: Buffer --> DVR(file)
    class Packager : public PacketizerSink
    {
    public:
        struct Config
        {
            uint32_t target_duration_ms = 6000;
			uint32_t max_segment_count = 10; // 10 segments, it will be saved in memory

			// Live Rewind
			ov::String dvr_storage_path;
			uint32_t dvr_window_ms = 0; // Rewind window in milliseconds,  1 hour (60000 * 60), it will be saved in files

			uint32_t segment_retention_count = 2; // This number of segments are retained event after the SegmentRemoved event occurs
        };

        Packager(const ov::String &packager_id, const Config &config);
        ~Packager();

		bool AddSink(const std::shared_ptr<PackagerSink> &sink);

        ////////////////////////////////
        // PacketizerSink interface
        ////////////////////////////////

        // PAT, PMT, ...
        void OnPsi(const std::vector<std::shared_ptr<const MediaTrack>> &tracks, const std::vector<std::shared_ptr<mpegts::Packet>> &psi_packets) override;
        // PES packets for a frame
        void OnFrame(const std::shared_ptr<const MediaPacket> &media_packet, const std::vector<std::shared_ptr<mpegts::Packet>> &pes_packets) override;

		// Get the segment data
		std::shared_ptr<Segment> GetSegment(uint32_t segment_id) const;
		std::shared_ptr<const ov::Data> GetSegmentData(uint32_t segment_id) const;

    private:
        const Config &GetConfig() const;

        uint32_t GetNextSegmentId();

        std::shared_ptr<ov::Data> MergeTsPacketData(const std::vector<std::shared_ptr<mpegts::Packet>> &ts_packets);
    
        std::shared_ptr<const MediaTrack> GetMediaTrack(uint32_t track_id) const;
        std::shared_ptr<SampleBuffer> GetSampleBuffer(uint32_t track_id) const;

        // Check if a segment is ready to be created and create it
        void CreateSegmentIfReady();
        void AddSegment(const std::shared_ptr<Segment> &segment);

		// Buffer
		void AddSegmentToBuffer(const std::shared_ptr<Segment> &segment);
		std::shared_ptr<Segment> GetOldestSegmentFromBuffer() const;
		size_t GetBufferedSegmentCount() const;
		void RemoveSegmentFromBuffer(const std::shared_ptr<Segment> &segment);

		// DVR
		void SaveSegmentToFile(const std::shared_ptr<Segment> &segment);
		void DeleteSegmentFile(const std::shared_ptr<Segment> &segment);
		void DeleteSegmentFromFileStoredList(const std::shared_ptr<Segment> &segment);
		uint64_t GetTotalFileStoredSegmentsDurationMs() const;
		std::shared_ptr<Segment> GetOldestSegmentFromFile() const;
		
		// Retention
		void SaveSegmentToRetentionBuffer(const std::shared_ptr<Segment> &segment);
		size_t GetReteinedSegmentCount() const;
		void RemoveSegmentFromRetentionBuffer(const std::shared_ptr<Segment> &segment);
		std::shared_ptr<Segment> GetOldestSegmentFromRetentionBuffer() const;

		// Broadcast
		void BroadcastSegmentCreated(const std::shared_ptr<Segment> &segment);
		void BroadcastSegmentDeleted(const std::shared_ptr<Segment> &segment);

		ov::String GetDvrStoragePath() const;
		ov::String GetSegmentFilePath(uint32_t segment_id) const;
        
        ov::String _packager_id;
        Config _config;

        // track_id -> SampleBuffer
        std::map<uint32_t, std::shared_ptr<SampleBuffer>> _sample_buffers;

        uint32_t _main_track_id = UINT32_MAX;
        std::map<uint32_t, std::shared_ptr<const MediaTrack>> _media_tracks;
        std::vector<std::shared_ptr<mpegts::Packet>> _psi_packets;
        std::shared_ptr<ov::Data> _psi_packet_data;

        uint32_t _last_segment_id = 0;

        std::map<uint32_t, std::shared_ptr<Segment>> _segments;
		uint64_t _total_segments_duration_ms = 0;
		mutable std::shared_mutex _segments_guard;

		std::map<uint32_t, std::shared_ptr<Segment>> _file_stored_segments;
		uint64_t _total_file_stored_segments_duration_ms = 0;
		mutable std::shared_mutex _file_stored_segments_guard;

		std::map<uint32_t, std::shared_ptr<Segment>> _retained_segments;
		mutable std::shared_mutex _retained_segments_guard;

		std::vector<std::shared_ptr<PackagerSink>> _sinks;
    };
}