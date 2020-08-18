#pragma once

// AudioSpecificConfig ()
// {
// audioObjectType; 5 bslbf
// samplingFrequencyIndex; 4 bslbf
// if ( samplingFrequencyIndex==0xf )
// samplingFrequency; 24 uimsbf
// channelConfiguration; 4 bslbf
// if ( audioObjectType == 1 || audioObjectType == 2 ||
// audioObjectType == 3 || audioObjectType == 4 ||
// audioObjectType == 6 || audioObjectType == 7 )
// GASpecificConfig();
// if ( audioObjectType == 8 )
// CelpSpecificConfig();
// if ( audioObjectType == 9 )
// HvxcSpecificConfig();
// if ( audioObjectType == 12 )
// TTSSpecificConfig();
// if ( audioObjectType == 13 || audioObjectType == 14 ||
// audioObjectType == 15 || audioObjectType==16)
// StructuredAudioSpecificConfig();
// if ( audioObjectType == 17 || audioObjectType == 19 ||
// audioObjectType == 20 || audioObjectType == 21 ||
// audioObjectType == 22 || audioObjectType == 23 )
// GASpecificConfig();
// if ( audioObjectType == 24)
// ErrorResilientCelpSpecificConfig();
// if ( audioObjectType == 25)
// ErrorResilientHvxcSpecificConfig();
// if ( audioObjectType == 26 || audioObjectType == 27)
// ParametricSpecificConfig();
// if ( audioObjectType == 17 || audioObjectType == 19 ||
// audioObjectType == 20 || audioObjectType == 21 ||
// audioObjectType == 22 || audioObjectType == 23 ||
// audioObjectType == 24 || audioObjectType == 25 ||
// audioObjectType == 26 || audioObjectType == 27 ) {
// epConfig; 2 bslbf
// if ( epConfig == 2 || epConfig == 3 ) {
// ErrorProtectionSpecificConfig();
// }
// ISO/IEC 14496-3:2001(E)
// 22 © ISO/IEC 2001— All rights reserved
// if ( epConfig == 3 ) {
// directMapping; 1 bslbf
// if ( ! directMapping ) {
// /* tbd */
// }
// }
// }
// }

// GASpecificConfig ( samplingFrequencyIndex,
// channelConfiguration,
// audioObjectType )
// {
// FrameLength; 1 bslbf
// DependsOnCoreCoder; 1 bslbf
// if ( dependsOnCoreCoder ) {
// coreCoderDelay; 14 uimsbf
// }
// ExtensionFlag 1 bslbf
// if ( ! ChannelConfiguration ) {
// program_config_element ();
// }
// if ( extensionFlag ) {
// if ( AudioObjectType==22 ) {
// numOfSubFrame 5 bslbf
// layer_length 11 bslbf
// }
// If(AudioObjectType==17 || AudioObjectType == 18 ||
// AudioObjectType == 19 || AudioObjectType == 20 ||
// AudioObjectType == 21 || AudioObjectType == 23){
// AacSectionDataResilienceFlag; 1 bslbf
// AacScalefactorDataResilienceFlag; 1 bslbf
// AacSpectralDataResilienceFlag; 1 bslbf
// }
// extensionFlag3; 1 bslbf
// if ( extensionFlag3 ) {
// /* tbd in version 3 */
// }
// }
// }

#include <base/ovlibrary/ovlibrary.h>

// Sampling Frequencies
enum SamplingFrequencies : uint8_t
{
	Samplerate_96000 = 0,
	Samplerate_88200 = 1,
	Samplerate_64000 = 2,
	Samplerate_48000 = 3,
	Samplerate_44100 = 4,
	Samplerate_32000 = 5,
	Samplerate_24000 = 6,
	Samplerate_22050 = 7,
	Samplerate_16000 = 8,
	Samplerate_12000 = 9,
	Samplerate_11025 = 10,
	Samplerate_8000 = 11,
	Samplerate_7350 = 12,
	Samplerate_Unknown = 13
};

enum class WellKnownAACObjectTypes : uint8_t
{
	NULLL = 0,
	AAC_MAIN = 1,
	AAC_LC = 2,
	AAC_SSR = 3,
	AAC_LTP = 4,
	SBR = 5,
	AAC_SCALABLE = 6,
	// ...
	ESCAPE_VALUE = 31
};



// MPEG-4 Audio Object Types:
enum AacObjectType : uint8_t
{
	// Table 1.1 - Audio Object Type definition
	// @see @see aac-mp4a-format-ISO_IEC_14496-3+2001.pdf, page 23
	AacObjectTypeAacMain = 1,
	AacObjectTypeAacLC = 2,
	AacObjectTypeAacSSR = 3,
	AacObjecttypeAacLTP = 4,
	AacObjectTypeAacHE = 5, // AAC HE = LC+SBR
	AacObjectTypeAacHEV2 = 29 // AAC HEv2 = LC+SBR+PS
};

enum class AACSamplingFrequencies : uint8_t
{
	RATES_96000HZ = 0,
	RATES_88200HZ = 1,
	RATES_64000HZ = 2,
	RATES_48000HZ = 3,
	RATES_44100HZ = 4,
	RATES_32000HZ = 5,
	RATES_24000HZ = 6,
	RATES_22050HZ = 7,
	RATES_16000HZ = 8,
	RATES_12000HZ = 9,
	RATES_11025HZ = 10,
	RATES_8000HZ = 11,
	RATES_7350HZ = 12,
	EXPLICIT_RATE = 15,
};

enum AacProfile
{
    AacProfileReserved = 3,
    
    // @see 7.1 Profiles, aac-iso-13818-7.pdf, page 40
    AacProfileMain = 0,
    AacProfileLC = 1,
    AacProfileSSR = 2,
};

#define MIN_AAC_SPECIFIC_CONFIG_SIZE		2

class AACSpecificConfig
{
public:
	static bool Parse(const uint8_t *data, size_t data_length, AACSpecificConfig &config);

	WellKnownAACObjectTypes		ObjectType();
	AACSamplingFrequencies		SamplingFrequency();
	uint8_t						Channel();
	ov::String 					GetInfoString();

	std::vector<uint8_t> Serialize() const;
	bool Deserialize(const std::vector<uint8_t> &stream);


private:
	WellKnownAACObjectTypes		_object_type;					// 5 bits
	AACSamplingFrequencies		_sampling_frequency_index;		// 4 bits
	uint8_t						_channel;						// 4 bits

	// GASpecificConfig (General Audio Coding, if object_type == AAC | tWINvq | BSAC)
	// we don't need this information
};