//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2022 AirenSoft. All rights reserved.
//
//==============================================================================

#include "fmp4_packager.h"
#include "fmp4_private.h"

#include <modules/bitstream/nalu/nal_stream_converter.h>
#include <modules/bitstream/aac/aac_converter.h>

#include <modules/id3v2/id3v2.h>
#include <modules/id3v2/frames/id3v2_text_frame.h>

namespace bmff
{
	FMP4Packager::FMP4Packager(const std::shared_ptr<FMP4Storage> &storage, const std::shared_ptr<const MediaTrack> &media_track, const std::shared_ptr<const MediaTrack> &data_track, const Config &config)
		: Packager(media_track, data_track, config.cenc_property)
	{
		_storage = storage;
		_config = config;

		_target_chunk_duration_ms = _config.chunk_duration_ms;
	}

	FMP4Packager::~FMP4Packager()
	{
		logtd("FMP4Packager has been terminated finally");
	}

	// Generate Initialization FMP4Segment
	bool FMP4Packager::CreateInitializationSegment()
	{
		auto track = GetMediaTrack();
		if (track == nullptr)
		{
			logte("Failed to create initialization segment. Track is null");
			return false;
		}

		if (track->GetCodecId() == cmn::MediaCodecId::H264 || 
			track->GetCodecId() == cmn::MediaCodecId::H265 ||
			track->GetCodecId() == cmn::MediaCodecId::Aac)
		{
			// Supported codecs
		}
		else
		{
			logtw("FMP4Packager::Initialize() - Unsupported codec id(%s)", StringFromMediaCodecId(track->GetCodecId()).CStr());
			return false;
		}

		// Create Initialization FMP4Segment
		ov::ByteStream stream(4096);
		
		if (WriteFtypBox(stream) == false)
		{
			logte("FMP4Packager::Initialize() - Failed to write ftyp box");
			return false;
		}

		if (WriteMoovBox(stream) == false)
		{
			logte("FMP4Packager::Initialize() - Failed to write moov box");
			return false;
		}

		return StoreInitializationSection(stream.GetDataPointer());
	}

	bool FMP4Packager::ReserveDataPacket(const std::shared_ptr<const MediaPacket> &media_packet)
	{
		if (GetDataTrack() == nullptr)
		{
			return false;
		}

		_reserved_data_packets.emplace(media_packet);
		return true;
	}

	std::shared_ptr<bmff::Samples> FMP4Packager::GetDataSamples(int64_t start_timestamp, int64_t end_timestamp)
	{
		if (GetDataTrack() == nullptr)
		{
			return nullptr;
		}

		auto rescaled_start_timestamp = ((double)start_timestamp / (double)GetMediaTrack()->GetTimeBase().GetTimescale()) * (double)GetDataTrack()->GetTimeBase().GetTimescale();
		auto rescaled_end_timestamp = ((double)end_timestamp / (double)GetMediaTrack()->GetTimeBase().GetTimescale()) * (double)GetDataTrack()->GetTimeBase().GetTimescale();

		auto samples = std::make_shared<Samples>();

		while (true)
		{
			if (_reserved_data_packets.size() == 0)
			{
				break;
			}

			auto data_packet = _reserved_data_packets.front();

			// Convert data pts timescale to media timescale
			auto pts = (double)data_packet->GetPts();

			logtd("track(%d), pts: %lf, start_timestamp: %lf, end_timestamp: %lf", GetMediaTrack()->GetId(), pts, rescaled_start_timestamp, rescaled_end_timestamp);

			if (pts == -1)
			{
				// Packets that must be inserted immediately
				auto copy_data_packet = data_packet->ClonePacket();
				copy_data_packet->SetPts(rescaled_start_timestamp);
				copy_data_packet->SetDts(rescaled_start_timestamp);
				
				samples->AppendSample(Sample(copy_data_packet));

				_reserved_data_packets.pop();
			}
			else if (pts > rescaled_end_timestamp)
			{
				//Waits for a segment within a time interval.
				break;
			}
			else if (pts < rescaled_start_timestamp)
			{
				// Too old data, ajust to start_timestamp
				_reserved_data_packets.pop();
			}
			else
			{
				// Within the time interval
				samples->AppendSample(data_packet);

				_reserved_data_packets.pop();
			}
		}

		return samples;
	}

	// Generate Media FMP4Segment
	bool FMP4Packager::AppendSample(const std::shared_ptr<const MediaPacket> &media_packet)
	{
		logtd("MediaPacket : track(%d) pts(%lld), dts(%lld), duration(%lld), flag(%d), size(%d)", media_packet->GetTrackId(), media_packet->GetPts(), media_packet->GetDts(), media_packet->GetDuration(), media_packet->GetFlag(), media_packet->GetData()->GetLength());

		// Convert bitstream format
		auto next_frame = ConvertBitstreamFormat(media_packet);
		if (next_frame == nullptr || next_frame->GetData() == nullptr)
		{
			logtw("Failed to convert bitstream format for track(%d)", GetMediaTrack()->GetId());
			return false;
		}

		std::shared_ptr<Samples> samples = _sample_buffer.GetSamples();

		if (samples != nullptr && samples->GetTotalCount() > 0)
		{
			// https://datatracker.ietf.org/doc/html/draft-pantos-hls-rfc8216bis#section-4.4.3.8
			// The duration of a Partial Segment MUST be less than or equal to the Part Target Duration.  
			// The duration of each Partial Segment MUST be at least 85% of the Part Target Duration, 
			// with the exception of Partial Segments with the INDEPENDENT=YES attribute 
			// and the final Partial Segment of any Parent Segment.

			auto last_segment = _storage->GetLastSegment();
			bool last_partial_segment = false;
			if (last_segment != nullptr && last_segment->IsCompleted() == false)
			{
				// https://datatracker.ietf.org/doc/html/draft-pantos-hls-rfc8216bis#section-4.4.4.9
				// The duration of a Partial Segment MUST be less than or equal to the
				// Part Target Duration.  The duration of each Partial Segment MUST be
				// at least 85% of the Part Target Duration, with the exception of
				// Partial Segments with the INDEPENDENT=YES attribute and the final
				// Partial Segment of any Parent Segment.
				if (_target_chunk_duration_ms + last_segment->GetDuration() >= _storage->GetTargetSegmentDuration())
				{
					// Last partial segment
					last_partial_segment = true;
				}
			}

			bool next_frame_is_idr = (next_frame->GetFlag() == MediaPacketFlag::Key) || (GetMediaTrack()->GetMediaType() == cmn::MediaType::Audio);

			// Calculate duration as milliseconds
			double total_duration = samples->GetTotalDuration();
			double expected_duration = total_duration + next_frame->GetDuration();

			double total_duration_ms = (static_cast<double>(total_duration) / GetMediaTrack()->GetTimeBase().GetTimescale()) * 1000.0;
			double expected_duration_ms = (static_cast<double>(expected_duration) / GetMediaTrack()->GetTimeBase().GetTimescale()) * 1000.0;

			logtd("track(%d), total_duration_ms: %lf, expected_duration_ms: %lf, target_chunk_duration_ms: %lf, next_frame_is_idr: %d, last_partial_segment: %d last_segment_duration: %lf, target_segment_duration: %lld", GetMediaTrack()->GetId(), total_duration_ms, expected_duration_ms, _target_chunk_duration_ms, next_frame_is_idr, last_partial_segment, last_segment != nullptr ? last_segment->GetDuration() : -1, _storage->GetTargetSegmentDuration());

			// - In the last partial segment, if the next frame is a keyframe, a segment is created immediately. This allows the segment to start with a keyframe.
			// - When adding samples, if the Part Target Duration is exceeded, a chunk is created immediately.
			// - If it exceeds 85% and the next sample is independent, a chunk is created. This makes the next chunk start independent.
			if ( 
				   (last_partial_segment == true && 
					GetMediaTrack()->GetMediaType() == cmn::MediaType::Video && 
					next_frame->GetFlag() == MediaPacketFlag::Key)

				|| (total_duration_ms >= _target_chunk_duration_ms) 

				|| ((expected_duration_ms > _target_chunk_duration_ms) && (total_duration_ms >= _target_chunk_duration_ms * 0.85)) 
				)
			{
				double reserve_buffer_size;
				
				if (GetMediaTrack()->GetMediaType() == cmn::MediaType::Video)
				{
					// Reserve 10 Mbps.
					reserve_buffer_size = (_target_chunk_duration_ms / 1000.0) * ((10.0 * 1000.0 * 1000.0) / 8.0);
				}
				else
				{
					// Reserve 0.5 Mbps.
					reserve_buffer_size = (_target_chunk_duration_ms / 1000.0) * ((0.5 * 1000.0 * 1000.0) / 8.0);
				}

				ov::ByteStream chunk_stream(reserve_buffer_size);
				
				auto data_samples = GetDataSamples(samples->GetStartTimestamp(), samples->GetEndTimestamp());
				if (data_samples != nullptr)
				{
					if (WriteEmsgBox(chunk_stream, data_samples) == false)
					{
						logtw("FMP4Packager::AppendSample() - Failed to write emsg box");
					}
				}

				if (WriteMoofBox(chunk_stream, samples) == false)
				{
					logte("FMP4Packager::AppendSample() - Failed to write moof box");
					return false;
				}

				if (WriteMdatBox(chunk_stream, samples) == false)
				{
					logte("FMP4Packager::AppendSample() - Failed to write mdat box");
					return false;
				}

				auto chunk = chunk_stream.GetDataPointer();

				if (_storage != nullptr && _storage->AppendMediaChunk(chunk, 
												samples->GetStartTimestamp(), 
												total_duration_ms, 
												samples->IsIndependent(), (last_partial_segment && next_frame_is_idr)) == false)
				{
					logte("FMP4Packager::AppendSample() - Failed to store media chunk");
					return false;
				}

				_sample_buffer.Reset();

				// Set the average chunk duration to config.chunk_duration_ms
				// _target_chunk_duration_ms -= total_duration_ms;
				// _target_chunk_duration_ms += _config.chunk_duration_ms;
			}
		}

		if (_sample_buffer.AppendSample(next_frame) == false)
		{
			logte("FMP4Packager::AppendSample() - Failed to append sample");
			return false;
		}

		return true;
	}

	// Get config
	const FMP4Packager::Config &FMP4Packager::GetConfig() const
	{
		return _config;
	}

	bool FMP4Packager::StoreInitializationSection(const std::shared_ptr<ov::Data> &section)
	{
		if (section == nullptr || _storage == nullptr)
		{
			return false;
		}

		if (_storage->StoreInitializationSection(section) == false)
		{
			return false;
		}

		return true;
	}

	

	std::shared_ptr<const MediaPacket> FMP4Packager::ConvertBitstreamFormat(const std::shared_ptr<const MediaPacket> &media_packet)
	{
		auto converted_packet = media_packet;

		// fmp4 uses avcC format
		if (media_packet->GetBitstreamFormat() == cmn::BitstreamFormat::H264_AVCC)
		{

		}
		else if (media_packet->GetBitstreamFormat() == cmn::BitstreamFormat::HVCC)
		{

		}
		else if (media_packet->GetBitstreamFormat() == cmn::BitstreamFormat::H264_ANNEXB)
		{
			auto converted_data = NalStreamConverter::ConvertAnnexbToXvcc(media_packet->GetData(), media_packet->GetFragHeader());
			if (converted_data == nullptr)
			{
				logtw("FMP4Packager::ConvertBitstreamFormat() - Failed to convert annexb to avcc");
				return nullptr;
			}

			auto new_packet = std::make_shared<MediaPacket>(*media_packet);
			new_packet->SetData(converted_data);
			new_packet->SetBitstreamFormat(cmn::BitstreamFormat::H264_AVCC);
			new_packet->SetPacketType(cmn::PacketType::NALU);

			converted_packet = new_packet;
		}
		else if (media_packet->GetBitstreamFormat() == cmn::BitstreamFormat::H265_ANNEXB)
		{
			auto converted_data = NalStreamConverter::ConvertAnnexbToXvcc(media_packet->GetData(), media_packet->GetFragHeader());
			if (converted_data == nullptr)
			{
				logtw("FMP4Packager::ConvertBitstreamFormat() - Failed to convert annexb to hvcc");
				return nullptr;
			}

			auto new_packet = std::make_shared<MediaPacket>(*media_packet);
			new_packet->SetData(converted_data);
			new_packet->SetBitstreamFormat(cmn::BitstreamFormat::HVCC);
			new_packet->SetPacketType(cmn::PacketType::NALU);

			converted_packet = new_packet;
		}
		else if (media_packet->GetBitstreamFormat() == cmn::BitstreamFormat::AAC_ADTS)
		{
			auto raw_data = AacConverter::ConvertAdtsToRaw(media_packet->GetData(), nullptr);
			if (raw_data == nullptr)
			{
				logtw("FMP4Packager::ConvertBitstreamFormat() - Failed to convert adts to raw");
				return nullptr;
			}

			auto new_packet = std::make_shared<MediaPacket>(*media_packet);
			new_packet->SetData(raw_data);
			new_packet->SetBitstreamFormat(cmn::BitstreamFormat::AAC_RAW);
			new_packet->SetPacketType(cmn::PacketType::RAW);

			converted_packet = new_packet;
		}
		else if (media_packet->GetBitstreamFormat() == cmn::BitstreamFormat::AAC_RAW)
		{

		}
		else
		{
			// Not supported yet
		}

		return converted_packet;
	}	

	bool FMP4Packager::WriteFtypBox(ov::ByteStream &data_stream)
	{
		ov::ByteStream stream(128);

		stream.WriteText("iso6"); // major brand
		stream.WriteBE32(0); // minor version
		stream.WriteText("iso6mp42avc1dashhlsfaid3"); // compatible brands

		// stream.WriteText("mp42"); // major brand
		// stream.WriteBE32(0); // minor version
		// stream.WriteText("isommp42iso5dash"); // compatible brands
		
		return WriteBox(data_stream, "ftyp", *stream.GetData());
	}
} // namespace bmff