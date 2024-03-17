#pragma once

// The Audio Specific Config is the global header for MPEG-4 Audio:
//////////////////////////////////////////////////////////////////////////////////////////
// Pseudo Code
//////////////////////////////////////////////////////////////////////////////////////////
// AudioSpecificConfig ()
// {
// 	audioObjectType; 5 bslbf
// 	samplingFrequencyIndex; 4 bslbf
// 	if ( samplingFrequencyIndex==0xf )
// 	samplingFrequency; 24 uimsbf
// 	channelConfiguration; 4 bslbf
// 	if ( audioObjectType == 1 || audioObjectType == 2 || audioObjectType == 3 || audioObjectType == 4 || audioObjectType == 6 || audioObjectType == 7 )
// 		GASpecificConfig();
// 	if ( audioObjectType == 8 )
// 		CelpSpecificConfig();
// 	if ( audioObjectType == 9 )
// 		HvxcSpecificConfig();
// 	if ( audioObjectType == 12 )
// 		TTSSpecificConfig();
// 	if ( audioObjectType == 13 || audioObjectType == 14 || audioObjectType == 15 || audioObjectType==16)
// 		StructuredAudioSpecificConfig();
// 	if ( audioObjectType == 17 || audioObjectType == 19 || audioObjectType == 20 || audioObjectType == 21 || audioObjectType == 22 || 
// 		 audioObjectType == 23 )
// 		GASpecificConfig();
// 	if ( audioObjectType == 24)
// 		ErrorResilientCelpSpecificConfig();
// 	if ( audioObjectType == 25)
// 		ErrorResilientHvxcSpecificConfig();
// 	if ( audioObjectType == 26 || audioObjectType == 27)
// 		ParametricSpecificConfig();
// 	if ( audioObjectType == 17 || audioObjectType == 19 || audioObjectType == 20 || audioObjectType == 21 || audioObjectType == 22 || 
// 		 audioObjectType == 23 || audioObjectType == 24 || audioObjectType == 25 ||	audioObjectType == 26 || audioObjectType == 27 ) 
// 	{
// 		epConfig; 2 bslbf
// 		if ( epConfig == 2 || epConfig == 3 ) 
// 		{
// 			ErrorProtectionSpecificConfig();
// 		}
		
// 		//ISO/IEC 14496-3:2001(E)	22 © ISO/IEC 2001— All rights reserved
// 		if ( epConfig == 3 ) {
// 			directMapping; 1 bslbf
// 			if ( ! directMapping ) {
// 				/* tbd */
// 			}
// 		}
// 	}
// }
// GASpecificConfig ( samplingFrequencyIndex, channelConfiguration, audioObjectType )
// {
// 	FrameLength; 1 bslbf
// 	DependsOnCoreCoder; 1 bslbf
// 	if ( dependsOnCoreCoder ) 
//	{
// 		coreCoderDelay; 14 uimsbf
// 	}
// 	ExtensionFlag 1 bslbf
// 	if ( ! ChannelConfiguration ) 
//	{
// 		program_config_element ();
// 	}
// 	if ( extensionFlag ) 
//	{
// 		if ( AudioObjectType==22 ) 
//		{
// 			numOfSubFrame 5 bslbf
// 			layer_length 11 bslbf
// 		}
// 		If(AudioObjectType==17 || AudioObjectType == 18 || AudioObjectType == 19 || AudioObjectType == 20 || AudioObjectType == 21 || 
// 		   AudioObjectType == 23)
//		{
// 			AacSectionDataResilienceFlag; 1 bslbf
// 			AacScalefactorDataResilienceFlag; 1 bslbf
// 			AacSpectralDataResilienceFlag; 1 bslbf
// 		}
// 		extensionFlag3; 1 bslbf
// 		if ( extensionFlag3 ) {
// 			/* tbd in version 3 */
// 		}
// 	}
// }
//
//////////////////////////////////////////////////////////////////////////////////////////

#include <base/ovlibrary/ovlibrary.h>
#include <base/info/decoder_configuration_record.h>

// MPEG-4 Audio Object Types:
enum AacObjectType : uint8_t
{
	AacObjectTypeNull = 0,
	// Table 1.1 - Audio Object Type definition
	// @see @see aac-mp4a-format-ISO_IEC_14496-3+2001.pdf, page 23
	AacObjectTypeAacMain = 1,
	AacObjectTypeAacLC = 2,
	AacObjectTypeAacSSR = 3,
	AacObjecttypeAacLTP = 4,
	AacObjectTypeAacHE = 5, // AAC HE = LC+SBR
	AacObjectTypeAacHEV2 = 29 // AAC HEv2 = LC+SBR+PS
};

enum AacProfile : uint8_t
{
	AacProfileReserved = 3,
	
	// @see 7.1 Profiles, aac-iso-13818-7.pdf, page 40
	AacProfileMain = 0,
	AacProfileLC = 1,
	AacProfileSSR = 2
};

enum AacSamplingFrequencies : uint8_t
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
	RATES_RESERVED = 13,
	EXPLICIT_RATE = 15
};

#define MIN_AAC_SPECIFIC_CONFIG_SIZE		2

class AudioSpecificConfig : public DecoderConfigurationRecord
{
public:
	bool IsValid() const override;
	// Instance can be initialized by putting raw data in AudioSpecificConfig.
	bool Parse(const std::shared_ptr<ov::Data> &data) override;
	bool Equals(const std::shared_ptr<DecoderConfigurationRecord> &other) override;

	std::shared_ptr<ov::Data> Serialize() override;

	AacObjectType				ObjectType() const;
	AacSamplingFrequencies		SamplingFrequency() const;
	uint32_t					SamplerateNum() const;
	uint8_t						Channel() const;
	
	AacProfile 					GetAacProfile() const;
	ov::String 					GetInfoString() const;

	void 						SetOjbectType(AacObjectType object_type);
	void 						SetSamplingFrequency(AacSamplingFrequencies sampling_frequency_index);
	void 						SetChannel(uint8_t channel);

	// Helpers
	ov::String GetCodecsParameter() const;

private:
	AacObjectType				_object_type = AacObjectType::AacObjectTypeNull; // 5 bits
	AacSamplingFrequencies		_sampling_frequency_index = AacSamplingFrequencies::RATES_RESERVED;	// 4 bits

	// 0 : Defined in AOT Specifc Config
	// 1 : 1 channel: front-center
	// 2 : 2 channels: front-left, front-right
	// 3 : 3 channels: front-center, front-left, front-right
	// 4 : 4 channels: front-center, front-left, front-right, back-center
	// ...
	// 8-15 : Reserved
	uint8_t						_channel = 15; // 4 bits
};