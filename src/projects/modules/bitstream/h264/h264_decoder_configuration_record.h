#pragma once

#include <base/ovlibrary/ovlibrary.h>

// ISO 14496-15, 5.2.4.1
//	aligned(8) class AVCDecoderConfigurationRecord {
//		unsigned int(8) configurationVersion = 1;
//		unsigned int(8) AVCProfileIndication;
//		unsigned int(8) profile_compatibility;
//		unsigned int(8) AVCLevelIndication;
//		bit(6) reserved = ‘111111’b;
//		unsigned int(2) lengthSizeMinusOne;
//		bit(3) reserved = ‘111’b;
//		unsigned int(5) numOfSequenceParameterSets;
//		for (i=0; i< numOfSequenceParameterSets;  i++) {
//			unsigned int(16) sequenceParameterSetLength ;
//			bit(8*sequenceParameterSetLength) sequenceParameterSetNALUnit;
//		}
//		unsigned int(8) numOfPictureParameterSets;
//		for (i=0; i< numOfPictureParameterSets;  i++) {
//			unsigned int(16) pictureParameterSetLength;
//			bit(8*pictureParameterSetLength) pictureParameterSetNALUnit;
//		}
//		if( profile_idc  ==  100  ||  profile_idc  ==  110  ||
//		    profile_idc  ==  122  ||  profile_idc  ==  144 )
//		{
//			bit(6) reserved = ‘111111’b;
//			unsigned int(2) chroma_format;
//			bit(5) reserved = ‘11111’b;
//			unsigned int(3) bit_depth_luma_minus8;
//			bit(5) reserved = ‘11111’b;
//			unsigned int(3) bit_depth_chroma_minus8;
//			unsigned int(8) numOfSequenceParameterSetExt;
//			for (i=0; i< numOfSequenceParameterSetExt; i++) {
//				unsigned int(16) sequenceParameterSetExtLength;
//				bit(8*sequenceParameterSetExtLength) sequenceParameterSetExtNALUnit;
//			}
//		}
//	}

#define MIN_AVCDECODERCONFIGURATIONRECORD_SIZE	7

class AVCDecoderConfigurationRecord
{
public:
	static bool Parse(const uint8_t *data, size_t data_length, AVCDecoderConfigurationRecord &record);

	uint8_t Version();
	uint8_t	ProfileIndication();
	uint8_t Compatibility();
	uint8_t LevelIndication();
	uint8_t LengthOfNALUnit();
	uint8_t NumOfSPS();
	uint8_t NumOfPPS();
	uint8_t NumofSPSExt();
	std::shared_ptr<ov::Data>	GetSPS(int index);
	std::shared_ptr<ov::Data>	GetPPS(int index);
	std::shared_ptr<ov::Data>	GetSPSExt(int index);
	uint8_t ChromaFormat();
	uint8_t BitDepthLumaMinus8();

	void Serialize(std::vector<uint8_t>& serialze);

	ov::String GetInfoString();
	
	void SetVersion(uint8_t version);
	void SetProfileIndication(uint8_t profile_indiciation);
	void SetCompatibility(uint8_t profile_compatibility);
	void SetlevelIndication(uint8_t level_indication);
	void SetLengthOfNalUnit(uint8_t lengthMinusOne);
	void AddSPS(std::shared_ptr<ov::Data> sps);
	void AddPPS(std::shared_ptr<ov::Data> pps);
	
private:
	uint8_t		_version = 0;
	uint8_t		_profile_indication = 0;
	uint8_t		_profile_compatibility = 0;
	uint8_t		_level_indication = 0;
	uint8_t		_reserved1 = 0;			// (6 bits) = 111111b
	uint8_t		_lengthMinusOne = 0;	// (2 bits)	= length of the NALUnitLength 0, 1, 3 correspoding to 1, 2, 4 (Usually 3)
	uint8_t		_reserved2 = 0;			// (3 bits) = 111b
	
	// for(int i=0; i<_num_of_sps; i++)
		// sps_length(16 bits) + sps
	uint8_t		_num_of_sps = 0;		// (5 bits)
	std::vector<std::shared_ptr<ov::Data>>	_sps_list;

	// for(int i=0; i<_num_of_pps; i++)
		// pps_length(16 bits) + sps
	uint8_t		_num_of_pps = 0;		// (8 bits)
	std::vector<std::shared_ptr<ov::Data>>	_pps_list;

	// if _profile_indication == 100 or 110 or 122 or 144
	uint8_t		_reserved3 = 0;			// (6 bits) = 111111b
	uint8_t		_chroma_format = 0;		// (2 bits)
	uint8_t		_reserved4 = 0;			// (5 bits) = 11111b
	uint8_t		_bit_depth_luma_minus8 = 0;	// (3 bits)
	
	// for(int i=0; i<_num_of_sps_ext; i++)
		// sps_ext_length(16 bits) + sps_ext
	uint8_t		_num_of_sps_ext = 0;		// (8 bits)
	std::vector<std::shared_ptr<ov::Data>>	_sps_ext_list;
};