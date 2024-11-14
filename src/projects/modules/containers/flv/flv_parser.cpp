#include "flv_parser.h"

#include <base/ovlibrary/bit_reader.h>
#include <base/ovlibrary/ovlibrary.h>

#define OV_LOG_TAG "FlvParser"

bool FlvVideoData::Parse(const std::shared_ptr<const ov::Data>& data)
{
	if (data->GetLength() < MIN_FLV_VIDEO_DATA_LENGTH)
	{
		logte("The data inputted is too small for parsing (%zu must be bigger than %d)", data->GetLength(), MIN_FLV_VIDEO_DATA_LENGTH);
		return false;
	}

	BitReader parser(data);

	_frame_type = static_cast<FlvVideoFrameTypes>(parser.ReadBits<uint8_t>(4));
	_codec_id = static_cast<FlvVideoCodecId>(parser.ReadBits<uint8_t>(4));

	if (_codec_id != FlvVideoCodecId::AVC)
	{
		logte("Unsupported codec : %d", static_cast<uint8_t>(_codec_id));
		return false;
	}

	_packet_type = static_cast<FlvAvcPacketType>(parser.ReadBytes<uint8_t>());

	int32_t composition_time = static_cast<int32_t>(parser.ReadBits<uint32_t>(24));

	// Need to convert UI24 to SI24
	_composition_time = OV_CHECK_FLAG(composition_time, 0x800000) ? composition_time |= 0xFF000000 : composition_time;
	_payload = parser.CurrentPosition();
	_payload_length = parser.BytesRemained();

	return true;
}

FlvVideoFrameTypes FlvVideoData::FrameType()
{
	return _frame_type;
}

FlvVideoCodecId FlvVideoData::CodecId()
{
	return _codec_id;
}

FlvAvcPacketType FlvVideoData::PacketType()
{
	return _packet_type;
}

int64_t FlvVideoData::CompositionTime()
{
	return _composition_time;
}

const uint8_t* FlvVideoData::Payload()
{
	return _payload;
}
size_t FlvVideoData::PayloadLength()
{
	return _payload_length;
}

bool FlvAudioData::Parse(const std::shared_ptr<const ov::Data>& data)
{
	if (data->GetLength() < MIN_FLV_AUDIO_DATA_LENGTH)
	{
		logte("The data inputted is too small for parsing (%zu must be bigger than %d)", data->GetLength(), MIN_FLV_AUDIO_DATA_LENGTH);
		return false;
	}

	BitReader parser(data);

	_format = static_cast<FlvSoundFormat>(parser.ReadBits<uint8_t>(4));
	_sample_rate = static_cast<FlvSoundRate>(parser.ReadBits<uint8_t>(2));
	_sample_size = static_cast<FlvSoundSize>(parser.ReadBit());
	_channel = static_cast<FlvSoundType>(parser.ReadBit());
	_packet_type = static_cast<FlvAACPacketType>(parser.ReadBytes<uint8_t>());

	_payload = parser.CurrentPosition();
	_payload_length = parser.BytesRemained();

	return true;
}

FlvSoundFormat FlvAudioData::Format()
{
	return _format;
}

FlvSoundRate FlvAudioData::SampleRate()
{
	return _sample_rate;
}

FlvSoundSize FlvAudioData::SampleSize()
{
	return _sample_size;
}

FlvSoundType FlvAudioData::Channel()
{
	return _channel;
}

FlvAACPacketType FlvAudioData::PacketType()
{
	return _packet_type;
}

const uint8_t* FlvAudioData::Payload()
{
	return _payload;
}

size_t FlvAudioData::PayloadLength()
{
	return _payload_length;
}