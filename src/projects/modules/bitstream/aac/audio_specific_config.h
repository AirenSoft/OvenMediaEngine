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

#include <base/info/decoder_configuration_record.h>
#include <base/ovlibrary/ovlibrary.h>

// Table 1.3 – Audio Profiles definition
// @see ISO/IEC 14496-3 (2009), Information technology - Coding of audio-visual objects - Part 3: Audio
enum class AudioObjectType : uint8_t
{
	Null = 0,							  // Null
	AacMain = 1,						  // Main
	AacLc = 2,							  // Low Complexity
	AacSsr = 3,							  // Scalable Sample Rate
	AacLtp = 4,							  // Long Term Predictor
	Sbr = 5,							  // SBR Spectral Band Replication
	AacScalable = 6,					  // AAC Scalable
	Twinvq = 7,							  // Twin VQ Vector Quantizer
	Celp = 8,							  // Code Excited Linear Prediction
	Hvxc = 9,							  // Harmonic Vector eXcitation Coding
	Reserved10 = 10,					  // (reserved)
	Reserved11 = 11,					  // (reserved)
	Ttsi = 12,							  // Text to Speech Interface
	MainSynthetic = 13,					  // Main Synthetic
	WavetableSynthesis = 14,			  // Wavetable Synthesis
	GeneralMidi = 15,					  // General MIDI
	AlgorithmicSynthesisAndAudioFx = 16,  // Algorithmic Synthesis and Audio FX
	ErAacLc = 17,						  // Error Resilient (ER) AAC Low Complexity (LC)
	Reserved18 = 18,					  // (reserved)
	ErAacLtp = 19,						  // Error Resilient (ER) AAC Long Term Predictor (LTP)
	ErAacScalable = 20,					  // Error Resilient (ER) AAC scalable
	ErTwinvq = 21,						  // Error Resilient (ER) TwinVQ
	ErBsac = 22,						  // Error Resilient (ER) Bit Sliced Arithmetic Coding
	ErAacLd = 23,						  // Error Resilient (ER) AAC Low Delay
	ErCelp = 24,						  // Error Resilient (ER) Code Excited Linear Prediction
	ErHvxc = 25,						  // Error Resilient (ER) Harmonic Vector eXcitation Coding
	ErHiln = 26,						  // Error Resilient (ER) Harmonic and Individual Lines plus Noise
	ErParametric = 27,					  // Error Resilient (ER) Parametric
	Ssc = 28,							  // SinuSoidal Coding
	Ps = 29,							  // Parametric Stereo
	MpegSurround = 30,					  // MPEG Surround
	Escape = 31,						  // (escape)
	Layer1 = 32,						  // Layer-1 Audio
	Layer2 = 33,						  // Layer-2 Audio
	Layer3 = 34,						  // Layer-3 Audio
	Dst = 35,							  // Direct Stream Transfer
	Als = 36,							  // Audio Lossless Coding
	Sls = 37,							  // Scalable Lossless Coding
	SlsNonCore = 38,					  // Scalable Lossless Non-Core Audio
	ErAacEld = 39,						  // Error Resilient (ER) AAC Enhanced Low Delay
	SmrSimple = 40,						  // Symbolic Music Representation Simple
	SmrMain = 41,						  // Symbolic Music Representation Main
};

enum class AacProfile : uint8_t
{
	Reserved = 3,

	// @see 7.1 Profiles, aac-iso-13818-7.pdf, page 40
	Main = 0,
	LC = 1,
	SSR = 2
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

#define MIN_AAC_SPECIFIC_CONFIG_SIZE 2

class AudioSpecificConfig : public DecoderConfigurationRecord
{
public:
	bool IsValid() const override;
	// Instance can be initialized by putting raw data in AudioSpecificConfig.
	bool Parse(const std::shared_ptr<ov::Data> &data) override;
	bool Equals(const std::shared_ptr<DecoderConfigurationRecord> &other) override;

	std::shared_ptr<ov::Data> Serialize() override;

	AudioObjectType ObjectType() const;
	AacSamplingFrequencies SamplingFrequency() const;
	uint32_t SamplerateNum() const;
	uint8_t Channel() const;

	AacProfile GetAacProfile() const;
	ov::String GetInfoString() const;

	void SetObjectType(AudioObjectType object_type);
	void SetSamplingFrequency(AacSamplingFrequencies sampling_frequency_index);
	void SetChannel(uint8_t channel);

	// Helpers
	ov::String GetCodecsParameter() const;

private:
	AudioObjectType _object_type = AudioObjectType::Null;										// 5 bits
	AacSamplingFrequencies _sampling_frequency_index = AacSamplingFrequencies::RATES_RESERVED;	// 4 bits

	// 0 : Defined in AOT Specifc Config
	// 1 : 1 channel: front-center
	// 2 : 2 channels: front-left, front-right
	// 3 : 3 channels: front-center, front-left, front-right
	// 4 : 4 channels: front-center, front-left, front-right, back-center
	// ...
	// 8-15 : Reserved
	uint8_t _channel = 15;	// 4 bits
};