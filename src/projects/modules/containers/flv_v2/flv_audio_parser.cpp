//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2025 AirenSoft. All rights reserved.
//
//==============================================================================
#include "./flv_audio_parser.h"

#include "./flv_private.h"

namespace modules
{
	namespace flv
	{
		bool IsValidSoundFormat(flv::SoundFormat format)
		{
			switch (format)
			{
				case flv::SoundFormat::LPcmPlatformEndian:
					[[fallthrough]];
				case flv::SoundFormat::AdPcm:
					[[fallthrough]];
				case flv::SoundFormat::Mp3:
					[[fallthrough]];
				case flv::SoundFormat::LPcmLittleEndian:
					[[fallthrough]];
				case flv::SoundFormat::Nellymoser16KMono:
					[[fallthrough]];
				case flv::SoundFormat::Nellymoser8KMono:
					[[fallthrough]];
				case flv::SoundFormat::Nellymoser:
					[[fallthrough]];
				case flv::SoundFormat::G711ALaw:
					[[fallthrough]];
				case flv::SoundFormat::G711MuLaw:
					[[fallthrough]];
				case flv::SoundFormat::ExHeader:
					[[fallthrough]];
				case flv::SoundFormat::Aac:
					[[fallthrough]];
				case flv::SoundFormat::Speex:
					[[fallthrough]];
				case flv::SoundFormat::Mp3_8K:
					[[fallthrough]];
				case flv::SoundFormat::Native:
					return true;
			}

			return false;
		}

		bool AudioParser::ParseLegacyAAC(ov::BitReader &reader, const std::shared_ptr<AudioData> &audio_data)
		{
			audio_data->aac_packet_type = reader.ReadU8As<AACPacketType>();
			audio_data->payload = reader.GetData();

			return true;
		}

		bool AudioParser::ProcessExAudioTagHeader(ov::BitReader &reader, SoundFormat sound_format, bool *process_audio_body)
		{
			// TODO: Need to implement
			OV_ASSERT2(false);

			return true;
		}

		bool AudioParser::Parse(ov::BitReader &reader)
		{
			try
			{
				// https://veovera.org/docs/enhanced/enhanced-rtmp-v2#enhanced-audio

				// soundFormat = UB[4] as SoundFormat
				auto sound_format = reader.ReadAs<SoundFormat>(4);

				_is_ex_header = (sound_format == SoundFormat::ExHeader);

				if (_is_ex_header == false)
				{
					auto audio_data = std::make_shared<AudioData>(sound_format);

					// See AudioTagHeader of the legacy [FLV] specification for for detailed format
					// of the four bits used for soundRate/soundSize/soundType
					//
					// NOTE: soundRate, soundSize and soundType formats have not changed.
					// if (soundFormat == SoundFormat.ExHeader) we switch into FOURCC audio mode
					// as defined below. This means that soundRate, soundSize and soundType
					// bits are not interpreted, instead the UB[4] bits are interpreted as an
					// AudioPacketType
					// soundRate = UB[2]
					// soundSize = UB[1]
					// soundType = UB[1]
					audio_data->sound_rate = reader.ReadAs<SoundRate>(2);
					audio_data->sound_size = reader.ReadAs<SoundSize>(1);
					audio_data->sound_type = reader.ReadAs<SoundType>(1);

					if (audio_data->sound_format != SoundFormat::Aac)
					{
						logtw("Sound format %d (%s) is not supported", audio_data->sound_format, EnumToString(audio_data->sound_format));
						return false;
					}

					// AAC
					ParseLegacyAAC(reader, audio_data);

					_data_list.push_back(audio_data);
				}
				else
				{
					bool process_audio_body = false;

					ProcessExAudioTagHeader(reader, sound_format, &process_audio_body);
				}

				return true;
			}
			catch (const ov::BitReaderError &e)
			{
				switch (static_cast<ov::BitReaderError::Code>(e.GetCode()))
				{
					case ov::BitReaderError::Code::NotEnoughBitsToRead:
						logtw("Not enough bits to read: %s - probably malformed audio data", e.GetMessage().CStr());
						break;

					case ov::BitReaderError::Code::NotEnoughSpaceInBuffer:
						// Internal server error
						logte("Not enough space in audio buffer: %s", e.GetMessage().CStr());
						OV_ASSERT2(false);
						break;

					case ov::BitReaderError::Code::FailedToReadBits:
						// Internal server error
						logte("Failed to read bits for audio: %s", e.GetMessage().CStr());
						OV_ASSERT2(false);
						break;
				}
			}

			return false;
		}
	}  // namespace flv
}  // namespace modules
