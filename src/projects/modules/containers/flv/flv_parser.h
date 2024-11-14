#pragma once

#include <base/ovlibrary/ovlibrary.h>

#define MIN_FLV_VIDEO_DATA_LENGTH		5

enum class FlvVideoFrameTypes : uint8_t
{
	KEY_FRAME = 1,	// For AVC
	INTER_FRAME = 2,	// For AVC
	DISPOSABLE_INTER_FRAME = 3, // H.263 Only
	GENERATED_KEY_FRAME = 4, // Reserved for server use only
	VIDEO_INFO_COMMAND_FRAME = 5,
};

enum class FlvVideoCodecId : uint8_t
{
	JPEG = 1,
	SORENSON_H_263,
	SCREEN_VIDEO,
	ON2_VP6,
	ON2_VP6_WITH_ALPHA_CHANNEL,
	SCREEN_VIDEO_VERSION_2,
	AVC	// OME supports only this codec on RTMP
};

enum class FlvAvcPacketType : uint8_t
{
	AVC_SEQUENCE_HEADER = 0, // AVCDecoderConfigurationRecord (ISO 14496-15, 5.2.4.1)
	AVC_NALU = 1,
	AVC_END_SEQUENCE = 2
};

#define MIN_FLV_AUDIO_DATA_LENGTH		2

enum class FlvSoundFormat : uint8_t
{
	LINEAR_PCM_PLATFORM_ENDIAN = 0,
	ADPCM = 1,
	MP3 = 2,
	LINEAR_PCM_LITTLE_ENDIAN = 3,
	NELLYMOSER_16KHZ_MONO = 4,
	NELLYMOSER_8KHZ_MONO = 5,
	NELLYMOSER = 6,
	G711_A_LAW_PCM = 7,
	G711_MU_LAW_PCM = 8,
	RESERVED = 9,
	AAC = 10, // OME supports only this sound format
	SPEEX = 11,
	MP3_8KHZ = 14,
	DEVICE_SPECIFIC_SOUND = 15
};

enum class FlvSoundRate : uint8_t
{
	KHZ_5_5 = 0,
	KHZ_11 = 1,
	KHZ_22 = 2,
	KHZ_44 = 3	// AAC always 3
};

enum class FlvSoundSize : uint8_t
{
	SND_8BIT = 0,
	SND_16BIT = 1	
};

enum class FlvSoundType : uint8_t
{
	SND_MONO = 0,
	SND_STEREO = 1 // AAC always 1
};

enum class FlvAACPacketType : uint8_t
{
	SEQUENCE_HEADER = 0, // AudioSpecificConfig (ISO 14496-3 1.6.2)
	RAW = 1
};

class FlvVideoData
{
public:
	bool Parse(const std::shared_ptr<const ov::Data> &data);

	FlvVideoFrameTypes FrameType();
	FlvVideoCodecId CodecId();
	FlvAvcPacketType PacketType();
	int64_t CompositionTime();

	const uint8_t*	Payload();
	size_t PayloadLength();

private:
	FlvVideoFrameTypes	_frame_type;		// UB[4]
	FlvVideoCodecId		_codec_id;			// UB[4], Only support AVC

	// AVCVIDEOPACKET
	FlvAvcPacketType	_packet_type;		// UI8
	int32_t				_composition_time;	// SI24

	// if FlvAvcPacketType == 0
	// 		AVCDecoderConfigurationRecord 
	// else if FlvAvcPacketType == 1
	//		One or more NALUs
	// else if FlvAvcPacketType == 2
	//		Empty

	const uint8_t*	_payload;
	size_t		_payload_length;
};

class FlvAudioData
{
public:
	bool Parse(const std::shared_ptr<const ov::Data> &data);

	FlvSoundFormat Format();
	FlvSoundRate SampleRate();
	FlvSoundSize SampleSize();
	FlvSoundType Channel();
	FlvAACPacketType PacketType();

	const uint8_t*	Payload();
	size_t PayloadLength();

private:
	FlvSoundFormat	_format;		// UB[4]
	FlvSoundRate _sample_rate;		// UB[2]
	FlvSoundSize _sample_size;		// UB[1]
	FlvSoundType _channel;			// UB[1]

	// format == AACAUDIODATA
	FlvAACPacketType _packet_type; 	// UI8

	// if packet_type == 0
	//		AudioSpecificConfig (ISO 14496-3 1.6.2)
	// else
	//		Raw AAC frame data
	const uint8_t*	_payload;
	size_t		_payload_length;
};