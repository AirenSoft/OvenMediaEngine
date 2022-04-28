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
		if (_samples_buffer == nullptr)
		{
			_samples_buffer = std::make_shared<Samples>();
		}

		if (_samples_buffer->AppendSample(media_packet) == false)
		{
			logte("FMP4Packager::AppendSample() - Failed to append sample");
			return false;
		}

		if (_samples_buffer->GetTotalDuration() >= _config.chunk_duration_ms)
		{
			auto reserve_buffer_size = ((double)GetConfig().chunk_duration_ms / 1000.0) * ((10.0 * 1000.0 * 1000.0) / 8.0);
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
			if (AppendMediaChunk(chunk, _samples_buffer->GetTotalDuration()) == false)
			{
				logte("FMP4Packager::AppendSample() - Failed to store media chunk");
				return false;
			}

			_samples_buffer.reset();

			return true;
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

	bool FMP4Packager::AppendMediaChunk(const std::shared_ptr<ov::Data> &chunk, uint32_t duration_ms)
	{
		if (chunk == nullptr || _storage == nullptr)
		{
			return false;
		}

		if (_storage->AppendMediaChunk(chunk, duration_ms) == false)
		{
			return false;
		}

		return true;
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