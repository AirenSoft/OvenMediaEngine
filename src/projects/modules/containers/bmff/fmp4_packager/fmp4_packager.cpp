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

#include <modules/bitstream/h264/h264_converter.h>
#include <modules/bitstream/aac/aac_converter.h>

namespace bmff
{
	FMP4Packager::FMP4Packager(const std::shared_ptr<FMP4Storage> &storage, const std::shared_ptr<const MediaTrack> &track, const Config &config)
		: Packager(track)
	{
		_storage = storage;
		_config = config;
	}

	// Generate Initialization FMP4Segment
	bool FMP4Packager::CreateInitializationSegment()
	{
		auto track = GetTrack();
		if (track == nullptr)
		{
			logte("Failed to create initialization segment. Track is null");
			return false;
		}

		if (track->GetCodecId() == cmn::MediaCodecId::H264 || 
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

	// Generate Media FMP4Segment
	bool FMP4Packager::AppendSample(const std::shared_ptr<const MediaPacket> &media_packet)
	{
		// Convert bitstream format
		auto converted_packet = ConvertBitstreamFormat(media_packet);
		if (converted_packet == nullptr)
		{
			// Never reached
			logtc("Failed to convert bitstream format");
			return false;
		}

		if (_samples_buffer != nullptr)
		{
			// https://datatracker.ietf.org/doc/html/draft-pantos-hls-rfc8216bis#section-4.4.3.8
			// The duration of a Partial Segment MUST be less than or equal to the Part Target Duration.  
			// The duration of each Partial Segment MUST be at least 85% of the Part Target Duration, 
			// with the exception of Partial Segments with the INDEPENDENT=YES attribute 
			// and the final Partial Segment of any Parent Segment.

			// Calculate duration as milliseconds
			uint64_t total_duration = _samples_buffer->GetTotalDuration();
			uint64_t expected_duration = total_duration + converted_packet->GetDuration();

			uint64_t total_duration_ms = (static_cast<double>(total_duration) / GetTrack()->GetTimeBase().GetTimescale()) * 1000.0;
			uint64_t expected_duration_ms = (static_cast<double>(expected_duration) / GetTrack()->GetTimeBase().GetTimescale()) * 1000.0;

			if (total_duration_ms > _config.chunk_duration_ms)
			{
				logtw("Too long duration chunk (%llu) was created. (configured : %llu) - - The duration of one frame may be too long, which may be longer than the configured chunk duration.", total_duration_ms, _config.chunk_duration_ms);
			}

			// 1. When adding samples, if the Part Target Duration is exceeded, a chunk is created immediately.
			// 2. If it exceeds 85% and the next sample is independent, a chunk is created. This makes the next chunk start independent.
			if ( (_samples_buffer->GetTotalCount() > 0 && expected_duration_ms > _config.chunk_duration_ms) ||
				(GetTrack()->GetMediaType() == cmn::MediaType::Video && total_duration_ms >= _config.chunk_duration_ms * 0.85 && converted_packet->GetFlag() == MediaPacketFlag::Key))
			{
				double reserve_buffer_size;
				
				if (GetTrack()->GetMediaType() == cmn::MediaType::Video)
				{
					// Reserve 10 Mbps.
					reserve_buffer_size = ((double)GetConfig().chunk_duration_ms / 1000.0) * ((10.0 * 1000.0 * 1000.0) / 8.0);
				}
				else
				{
					// Reserve 0.5 Mbps.
					reserve_buffer_size = ((double)GetConfig().chunk_duration_ms / 1000.0) * ((0.5 * 1000.0 * 1000.0) / 8.0);
				}

				ov::ByteStream chunk_stream(reserve_buffer_size);

				if (WriteMoofBox(chunk_stream, _samples_buffer) == false)
				{
					logte("FMP4Packager::AppendSample() - Failed to write moof box");
					return false;
				}

				if (WriteMdatBox(chunk_stream, _samples_buffer) == false)
				{
					logte("FMP4Packager::AppendSample() - Failed to write mdat box");
					return false;
				}

				auto chunk = chunk_stream.GetDataPointer();
				if (AppendMediaChunk(chunk, _samples_buffer->GetStartTimestamp(), total_duration_ms, _samples_buffer->IsIndependent()) == false)
				{
					logte("FMP4Packager::AppendSample() - Failed to store media chunk");
					return false;
				}

				_samples_buffer.reset();
			}
		}

		if (_samples_buffer == nullptr)
		{
			_samples_buffer = std::make_shared<Samples>();
		}

		if (_samples_buffer->AppendSample(converted_packet) == false)
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

	bool FMP4Packager::AppendMediaChunk(const std::shared_ptr<ov::Data> &chunk, uint64_t start_timestamp, uint32_t duration_ms, bool independent)
	{
		if (chunk == nullptr || _storage == nullptr)
		{
			return false;
		}

		if (_storage->AppendMediaChunk(chunk, start_timestamp, duration_ms, independent) == false)
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
		else if (media_packet->GetBitstreamFormat() == cmn::BitstreamFormat::H264_ANNEXB)
		{
			auto converted_data = H264Converter::ConvertAnnexbToAvcc(media_packet->GetData());
			auto new_packet = std::make_shared<MediaPacket>(*media_packet);
			new_packet->SetData(converted_data);
			new_packet->SetBitstreamFormat(cmn::BitstreamFormat::H264_AVCC);
			new_packet->SetPacketType(cmn::PacketType::NALU);

			converted_packet = new_packet;
		}
		else if (media_packet->GetBitstreamFormat() == cmn::BitstreamFormat::AAC_ADTS)
		{
			auto raw_data = AacConverter::ConvertAdtsToRaw(media_packet->GetData(), nullptr);
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
		stream.WriteText("iso6mp42avc1dashhlsf"); // compatible brands

		// stream.WriteText("mp42"); // major brand
		// stream.WriteBE32(0); // minor version
		// stream.WriteText("isommp42iso5dash"); // compatible brands
		
		return WriteBox(data_stream, "ftyp", *stream.GetData());
	}
} // namespace bmff