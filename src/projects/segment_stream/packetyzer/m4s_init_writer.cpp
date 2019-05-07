//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#include "m4s_init_writer.h"
#include "bit_writer.h"

//Fragmented MP4
// init m4s
//    - ftyp(File Type)
//    - moov(Movie Fragment)
//        - mvhd(Movie Header)
//        - trak(Track)
//            - tkhd(Track Header)
//            //- edts(Edit)
//            //    - elst(Edit List)
//            - media(Media)
//                - mdhd(Media Header)
//                - hdlr(Handler Reference)
//                - minf(Media Information)
//                    - vmhd(video)
//                    - smhd(Sound Media Header)
//                    - dinf(Data Information)
//                        - dref(Data Reference)
//                        - url
//                    - stbl(Sample Table)
//                        - stsd(Sample Description)
//                            - avc1(AVC Sample)
//                                - avcC(AVC Decoder Configuration Record)
//                        - mp4a(MPEG4 Audio Sample Entry)
//                            - esds(ES Descriptor)
//                        - stts(Decoding TIme to Sample)
//                        - stsc(Sample To Chunk)
//                        - stsz(Sample Size)
//                        - stco(Chunk Offset)
//        - mvex(Movie Extends)
//            - mehd(Movie Extends Header)
//            - trex(Track Extends)

const int g_sample_rate_table[] = {96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000, 7350, 0, 0, 0};
#define SAMPLERATE_TABLE_SIZE (16)

//====================================================================================================
// Constructor
//====================================================================================================
M4sInitWriter::M4sInitWriter(	M4sMediaType 	media_type,
								int 			data_init_size,
								uint32_t		duration,
								uint32_t		timescale,
								uint32_t		track_id,
								uint32_t		video_width,
								uint32_t		video_height,
								std::shared_ptr<std::vector<uint8_t>> &avc_sps,
								std::shared_ptr<std::vector<uint8_t>> &avc_pps,
								uint16_t        audio_channels,
								uint16_t        audio_sample_size,
								uint16_t        audio_sample_rate) : M4sWriter(media_type, data_init_size)
{	 
	_duration			= duration;
	_timescale			= timescale;
	_track_id			= track_id;

	_video_width		= video_width;
	_video_height		= video_height;
	_avc_sps			= avc_sps;
	_avc_pps			= avc_pps;

	_audio_channels     = audio_channels;
	_audio_sample_size  = audio_sample_size;
	_audio_sample_rate  = audio_sample_rate;
	_audio_sample_index = 4;

	for(uint32_t index = 0; index < SAMPLERATE_TABLE_SIZE; index++)
	{
		if(g_sample_rate_table[index] == audio_sample_rate)
		{
			_audio_sample_index = index;
			break;
		}
	}

	_language			    = "und";

	if (media_type == M4sMediaType::VideoMediaType)
	{
		_handler_type       = "vide";
		_compressor_name    = "OmeVideoHandler";
	}
	else if (media_type == M4sMediaType::AudioMediaType)
	{
		_handler_type       = "soun";
		_compressor_name    = "OmeAudioHandler";
	}
 }

//====================================================================================================
// ftyp(File Type) / Init Segment
//====================================================================================================
int M4sInitWriter::CreateData()
{
	int ftyp_size = 0; 
	int moov_size = 0; 

	ftyp_size += FtypBoxWrite(_data_stream);
	moov_size += MoovBoxWrite(_data_stream);

	return ftyp_size + moov_size;
}

//====================================================================================================
// ftyp(File Type) 
//====================================================================================================
int M4sInitWriter::FtypBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream)
{
	std::string major_brand = "mp42";
	std::string compatible_brands = "isommp42iso5dash";// isom(4)mp42(4)iso5(4)dash(4)
	std::shared_ptr<std::vector<uint8_t>> data = std::make_shared<std::vector<uint8_t>>(); 

	WriteText(major_brand, data);         // Major brand
	WriteUint32(0, data);                   // Minor version
	WriteText(compatible_brands, data);   // Compatible brands
	
	return BoxDataWrite("ftyp", data, data_stream);

}

//====================================================================================================
// Moov(Movie)
//====================================================================================================
int M4sInitWriter::MoovBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream)
{
	std::shared_ptr<std::vector<uint8_t>> data = std::make_shared<std::vector<uint8_t>>();

	MvhdBoxWrite(data);
	MvexBoxWrite(data);
	TrakBoxWrite(data);
	
	return BoxDataWrite("moov", data, data_stream);
}

//====================================================================================================
// mvhd(Movie Header)
//====================================================================================================
int M4sInitWriter::MvhdBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream)
{
	std::shared_ptr<std::vector<uint8_t>> data = std::make_shared<std::vector<uint8_t>>();
	std::vector<uint8_t> metrix = {0, 0x01, 0, 0,
								   0, 0, 0, 0,
								   0, 0, 0, 0,
								   0, 0, 0, 0,
								   0, 0x01, 0, 0,
								   0, 0, 0, 0,
								   0, 0, 0, 0,
								   0, 0, 0, 0,
								   0x40, 0, 0, 0};

	WriteUint32(0, data);               // Create Time
	WriteUint32(0, data);               // Modification Time
	WriteUint32(_timescale, data);      // Timescale
	WriteUint32(_duration, data);       // Duration
	WriteUint32(0x01 << 16, data);      // rate version
	WriteUint16(0x01 << 8, data);          // volme version
	WriteInit(0, 10, data);             // Reserve(10Byte)
	WriteData(metrix, data);            // Matrix
	WriteInit(0, 24, data);             // pre-defined(24byte)
	WriteUint32(0XFFFFFFFF, data);   // Next Track ID

	return BoxDataWrite("mvhd", 0, 0, data, data_stream);
}

//====================================================================================================
// trak(Track)
//====================================================================================================
int M4sInitWriter::TrakBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream)
{
	std::shared_ptr<std::vector<uint8_t>> data = std::make_shared<std::vector<uint8_t>>();

	TkhdBoxWrite(data);
	MdiaBoxWrite(data);

	return BoxDataWrite("trak", data, data_stream);
}

//====================================================================================================
// tkhd(Track Header) 
//====================================================================================================
int M4sInitWriter::TkhdBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream)
{
	std::shared_ptr<std::vector<uint8_t>> data = std::make_shared<std::vector<uint8_t>>();
	std::vector<uint8_t> metrix = {0, 0x01, 0, 0,
								   0, 0, 0, 0,
								   0, 0, 0, 0,
								   0, 0, 0, 0,
								   0, 0x01, 0, 0,
								   0, 0, 0, 0,
								   0, 0, 0, 0,
								   0, 0, 0, 0,
								   0x40, 0, 0, 0};

	WriteUint32(0, data);					// Create Time
	WriteUint32(0, data);					// Modification Time
	WriteUint32(_track_id, data);			// Track ID
	WriteInit(0, 4, data);					// Reserve(4Byte)
	WriteUint32(_duration, data);			// Duration
	WriteInit(0, 8, data);					// Reserve(8Byte)
	WriteUint16(0, data);					// layer
	WriteUint16(0, data);					// alternate group
	WriteUint16(0, data);					// volume
	WriteInit(0, 2, data);					// Reserve(2Byte)
	WriteData(metrix, data);				// Matrix

	if(_media_type == M4sMediaType::VideoMediaType)
	{
		WriteUint32(_video_width << 16, data);  // Width
		WriteUint32(_video_height << 16, data); // Height
	}
	else
	{
		WriteUint32(0, data);  // Width
		WriteUint32(0, data); // Height
	}

	return BoxDataWrite("tkhd", 0, 7, data, data_stream);
}

//====================================================================================================
// mdia(Media) 
//====================================================================================================
int M4sInitWriter::MdiaBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream)
{
	std::shared_ptr<std::vector<uint8_t>> data = std::make_shared<std::vector<uint8_t>>();

	MdhdBoxWrite(data);
	HdlrBoxWrite(data);
	MinfBoxWrite(data);

	return BoxDataWrite("mdia", data, data_stream);
}

//====================================================================================================
// mdhd(Media Information) 
//====================================================================================================
int M4sInitWriter::MdhdBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream)
{
	std::shared_ptr<std::vector<uint8_t>> data = std::make_shared<std::vector<uint8_t>>();

	WriteUint32(0, data);               // Create Time
	WriteUint32(0, data);               // Modification Time
	if (_media_type == M4sMediaType::VideoMediaType)
	{
		WriteUint32(_timescale, data);      // Timescale
		WriteUint32(_duration, data);       // Duration
	}
	else if(_media_type == M4sMediaType::AudioMediaType)
	{
		WriteUint32(_audio_sample_rate, data);  // Timescale
		WriteUint32(_duration, data);					// Duration
	}

	WriteUint8((((_language[0] - 0x60) << 2) | (_language[1] - 0x60) >> 3) & 0xFF , data); // Language 1
	WriteUint8((((_language[1] - 0x60) << 5) |  (_language[2] - 0x60)) & 0xFF , data);          // Language 2
	WriteUint16(0, data);					// Pre Define 

	return BoxDataWrite("mdhd", 0, 0, data, data_stream);
}

//====================================================================================================
// Hdlr(Handler Reference) 
//====================================================================================================
int M4sInitWriter::HdlrBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream)
{
	std::shared_ptr<std::vector<uint8_t>> data = std::make_shared<std::vector<uint8_t>>();

	WriteUint32(0, data);				// Pre Define
	WriteText(_handler_type, data);		// Handler Type
	WriteInit(0, 12, data);	            // Reserve(12Byte)
	WriteText(_compressor_name, data);  // Handler Name
	WriteUint8(0, data);				// null

	return BoxDataWrite("hdlr", 0, 0, data, data_stream);
}

//====================================================================================================
// minf(Media) 
//====================================================================================================
int M4sInitWriter::MinfBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream)
{
	std::shared_ptr<std::vector<uint8_t>> data = std::make_shared<std::vector<uint8_t>>();

	if(_media_type == M4sMediaType::VideoMediaType)     	VmhdBoxWrite(data);
	else if(_media_type == M4sMediaType::AudioMediaType)	SmhdBoxWrite(data);

	DinfBoxWrite(data); 
	StblBoxWrite(data);

	return BoxDataWrite("minf", data, data_stream);
}

//====================================================================================================
// vmhd(Video Media Header) 
//====================================================================================================
int M4sInitWriter::VmhdBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream)
{
	std::shared_ptr<std::vector<uint8_t>> data = std::make_shared<std::vector<uint8_t>>();

	WriteUint16(0, data);	// Graphics Mode
	WriteInit(0, 6, data);	// Op Color

	return BoxDataWrite("vmhd", 0, 1, data, data_stream);
}

//====================================================================================================
// smhd(Sound Media Header)
//====================================================================================================
int M4sInitWriter::SmhdBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream)
{
	std::shared_ptr<std::vector<uint8_t>> data = std::make_shared<std::vector<uint8_t>>();

	WriteUint16(0, data);	// Balance
	WriteUint16(0, data);	// Reserved

	return BoxDataWrite("smhd", 0, 0, data, data_stream);
}

//====================================================================================================
// dinf(Data Information) 
//====================================================================================================
int M4sInitWriter::DinfBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream)
{
	std::shared_ptr<std::vector<uint8_t>> data = std::make_shared<std::vector<uint8_t>>();

	DrefBoxWrite(data);

	return BoxDataWrite("dinf", data, data_stream);
}

//====================================================================================================
// dref(Data Reference)
//====================================================================================================
int M4sInitWriter::DrefBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream)
{
	std::shared_ptr<std::vector<uint8_t>> data = std::make_shared<std::vector<uint8_t>>();

	WriteUint32(1, data); // child count
	UrlBoxWrite(data);    // url child
   

	return BoxDataWrite("dref", 0, 0, data, data_stream);
}

//====================================================================================================
// url(Data Entry Url) 
//====================================================================================================
int M4sInitWriter::UrlBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream)
{
	std::shared_ptr<std::vector<uint8_t>> data = nullptr;

	return BoxDataWrite("url ", 0, 1, data, data_stream);
}

//====================================================================================================
// stbl(Sample Table)
//====================================================================================================
int M4sInitWriter::StblBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream)
{
	std::shared_ptr<std::vector<uint8_t>> data = std::make_shared<std::vector<uint8_t>>();

	StsdBoxWrite(data);
	SttsBoxWrite(data);
	StscBoxWrite(data);
	StszBoxWrite(data);
	StcoBoxWrite(data);

	return BoxDataWrite("stbl", data, data_stream);
}

//====================================================================================================
// stsd(Sample Description)
//====================================================================================================
int M4sInitWriter::StsdBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream)
{
	std::shared_ptr<std::vector<uint8_t>> data = std::make_shared<std::vector<uint8_t>>();

	WriteUint32(1, data); // Child Count

	if(_media_type == M4sMediaType::VideoMediaType)	Avc1BoxWrite(data);
	if(_media_type == M4sMediaType::AudioMediaType)	Mp4aBoxWrite(data);

	return BoxDataWrite("stsd", 0, 0, data, data_stream);
}

//====================================================================================================
// avc1(AVC Sample Entry)
//====================================================================================================
int M4sInitWriter::Avc1BoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream)
{
	std::shared_ptr<std::vector<uint8_t>> data = std::make_shared<std::vector<uint8_t>>();

	WriteUint32(1, data);								// Child Count
	WriteUint16(0, data);								// Pre Define
	WriteUint16(0, data);								// Reserve(2Byte)
	WriteInit(0, 12, data);								// Pre Define(12byte) 
	WriteUint16((uint16_t)_video_width, data);					// Width
	WriteUint16((uint16_t)_video_height, data);					// Height
	WriteUint32(0x00480000, data);						// Horiz Resolution
	WriteUint32(0x00480000, data);						// Vert Resolution
	WriteUint32(0, data);								// Reserve(4Byte)
	WriteUint16(1, data);								// Frame Count
	WriteUint8((uint8_t)_compressor_name.size(), data); // Compressor Name Size(Max 31Byte) 
	WriteText(_compressor_name, data);					// Compressor Name 
	WriteInit(0, 31 - _compressor_name.size(), data);	// Padding(31 - Compressor Name Size) 
	WriteUint16(0x0018, data);							// Depth
	WriteUint16(0xFFFF, data);							// Pre Define

	AvccBoxWrite(data); 

	return BoxDataWrite("avc1", 0, 0, data, data_stream);
}

//====================================================================================================
// avcC(AVC Decoder Configuration Record) 
//====================================================================================================
int M4sInitWriter::AvccBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream)
{
	std::shared_ptr<std::vector<uint8_t>> data = std::make_shared<std::vector<uint8_t>>();
	uint8_t avc_profile 				= _avc_sps->at(1);
	uint8_t avc_profile_compatibility 	= _avc_sps->at(2);
	uint8_t avc_level 					= _avc_sps->at(3);
	uint8_t avc_nal_unit_size			= 4;

	WriteUint8(1, data);							// Configuration Version
	WriteUint8(avc_profile, data);					// Profile
	WriteUint8(avc_profile_compatibility, data);	// Profile Compatibillity
	WriteUint8(avc_level, data);					// Level
	WriteUint8((uint8_t)((avc_nal_unit_size - 1) | 0xFC), data);	// Nal Unit Size
	WriteUint8(1 | 0xE0, data);						// SPS Count
	WriteUint16(_avc_sps->size(), data);				// SPS Size
	WriteData(*_avc_sps, data);						// SPS
	WriteUint8(1, data);							// PPS Count
	WriteUint16(_avc_pps->size(), data);				// PPS Size
	WriteData(*_avc_pps, data);						// PPS

	return BoxDataWrite("avcC", data, data_stream);
}
//====================================================================================================
// Mp4a(MPEG-4 Audio Sample Entry)
//====================================================================================================
int M4sInitWriter::Mp4aBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream)
{
	std::shared_ptr<std::vector<uint8_t>> data = std::make_shared<std::vector<uint8_t>>();

	WriteUint32(1, data);				            // Child Count
	WriteUint16(0, data);				            // QT version
	WriteUint16(0, data);				            // QT revision
	WriteUint32(0, data);				            // QT vendor
	WriteUint16(_audio_channels, data);        // channel count
	WriteUint16(_audio_sample_size, data);	        // sample size
	WriteUint16(0, data);				            // QT compression ID
	WriteUint16(0, data);				            // QT packet size
	WriteUint32(_audio_sample_rate << 16, data);	// sample rate

	EsdsBoxWrite(data);

	return BoxDataWrite("mp4a", 0, 0, data, data_stream);
}

//====================================================================================================
// esds(ES Description)
//====================================================================================================
int M4sInitWriter::EsdsBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream)
{
	std::shared_ptr<std::vector<uint8_t>> data = std::make_shared<std::vector<uint8_t>>();

	// es id(3)
	WriteUint8(3, data);							// tag
	WriteUint8(0x19, data);							// tag size
	WriteUint16(0, data);							// es id ??? track id
	WriteUint8(0, data);							// flag

	// decoder config(13)
	WriteUint8(4, data);							// tag
	WriteUint8(0x11, data);							// tag size
	WriteUint8(0x40, data);							// Object type indication  - MPEG-4 audio (0X40)
	WriteUint8(0x15, data);							// Stream type( <<2)  / Up Stream ( << 1) / Reserve(0x01)
	WriteUint24(0, data);							// Buffer Size
	WriteUint32(0, data);							// MaxBitrate
	WriteUint32(0, data);							// AverageBitreate

	// DecoderSpecific info descriptor
	/*
	   5 bits: object type
	   4 bits: frequency index
	   4 bits: channel configuration
	   1 bit: frame length flag
	   1 bit: dependsOnCoreCoder
	   1 bit: extensionFlag
		   Audio Object Types
		MPEG-4 Audio Object Types:
		0: Null
		1: AAC Main
		2: AAC LC (Low Complexity)
		3: AAC SSR (Scalable Sample Rate)
		4: ...
   */
	BitWriter bit_writer(2);
	bit_writer.Write(5, 2);						// object type - 2: AAC LC (Low Complexity)
	bit_writer.Write(4, _audio_sample_index);	// frequency index
	bit_writer.Write(4, _audio_channels);		// channel configuration

	WriteUint8(5, data);						// tag
	WriteUint8(2, data);						// tag size

	WriteData(bit_writer.GetData(), (int)bit_writer.GetDataSize(), data);    //

	// sl config(1)
	WriteUint8(6, data);	// tag
	WriteUint8(1, data);	// tag size
	WriteUint8(2, data);	// always 2 refer from mov_write_esds_tag

	return BoxDataWrite("esds", 0, 0, data, data_stream);
}

//====================================================================================================
// stts(Decoding Time to Sample) 
//====================================================================================================
int M4sInitWriter::SttsBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream)
{
	std::shared_ptr<std::vector<uint8_t>> data = std::make_shared<std::vector<uint8_t>>();

	WriteUint32(0, data); // Entry Count

	return BoxDataWrite("stts", 0, 0, data, data_stream);
}

//====================================================================================================
// stsc(Sample To Chunk) 
//====================================================================================================
int M4sInitWriter::StscBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream)
{
	std::shared_ptr<std::vector<uint8_t>> data = std::make_shared<std::vector<uint8_t>>();

	WriteUint32(0, data); // Entry Count

	return BoxDataWrite("stsc", 0, 0, data, data_stream);
}

//====================================================================================================
// stsz(Sample Size) 
//====================================================================================================
int M4sInitWriter::StszBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream)
{
	std::shared_ptr<std::vector<uint8_t>> data = std::make_shared<std::vector<uint8_t>>();

	WriteUint32(0, data); // Sample Size
	WriteUint32(0, data); // Sample Count

	return BoxDataWrite("stsz", 0, 0, data, data_stream);
}

//====================================================================================================
// stco(Chunk Offset)
//====================================================================================================
int M4sInitWriter::StcoBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream)
{
	std::shared_ptr<std::vector<uint8_t>> data = std::make_shared<std::vector<uint8_t>>();

	WriteUint32(0, data); // Entry Count

	return BoxDataWrite("stco", 0, 0, data, data_stream);
}

//====================================================================================================
// Mvex(Movie Extends) 
//====================================================================================================
int M4sInitWriter::MvexBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream)
{
	std::shared_ptr<std::vector<uint8_t>> data = std::make_shared<std::vector<uint8_t>>();

	//MehdBoxWrite(data);
	TrexBoxWrite(data);

	return BoxDataWrite("mvex", data, data_stream);
}

//====================================================================================================
// mehd(Movie Extends Header)
//====================================================================================================
int M4sInitWriter::MehdBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream)
{
	std::shared_ptr<std::vector<uint8_t>> data = std::make_shared<std::vector<uint8_t>>();

	return BoxDataWrite("mehd", 0, 0, data, data_stream);
}

//====================================================================================================
// trex(Track Extends) 
//====================================================================================================
int M4sInitWriter::TrexBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream)
{
	std::shared_ptr<std::vector<uint8_t>> data = std::make_shared<std::vector<uint8_t>>();

	WriteUint32(_track_id, data);   // Track ID
	WriteUint32(1, data);			// Sample Description Index
	WriteUint32(1, data);			// Sample Duration
	WriteUint32(1, data);			// Sample Size
	WriteUint32(0, data);			// Sample Flags

	return BoxDataWrite("trex", 0, 0, data, data_stream);
}
