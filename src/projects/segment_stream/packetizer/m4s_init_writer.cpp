//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Hyunjun Jang
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================
#include "m4s_init_writer.h"
#include "bit_writer.h"

#include <base/media_route/media_type.h>

#define SAMPLERATE_TABLE_SIZE (16)

M4sInitWriter::M4sInitWriter(M4sMediaType media_type,
							 uint32_t duration,
							 std::shared_ptr<MediaTrack> video_track, std::shared_ptr<MediaTrack> audio_track,
							 const std::shared_ptr<const ov::Data> &avc_sps, const std::shared_ptr<const ov::Data> &avc_pps)
	: M4sWriter(media_type),

	  _duration(duration),

	  _video_track(video_track),
	  _audio_track(audio_track),

	  _avc_sps(avc_sps),
	  _avc_pps(avc_pps)
{
	switch (media_type)
	{
		case M4sMediaType::Video:
			_main_track = _video_track;
			_track_id = 1;

			_handler_type = "vide";
			_compressor_name = "OvenMediaEngine";

			break;

		case M4sMediaType::Audio:
			_main_track = _audio_track;
			_track_id = 2;

			_handler_type = "soun";
			_compressor_name = "OvenMediaEngine";
			break;

		case M4sMediaType::Data:
			break;
	}

	if (_main_track == nullptr)
	{
		OV_ASSERT2(false);
	}

	if (_audio_track != nullptr)
	{
		const int sample_rate_table[] = {96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000, 7350, 0, 0, 0};

		for (uint32_t index = 0; index < SAMPLERATE_TABLE_SIZE; index++)
		{
			if (sample_rate_table[index] == audio_track->GetSampleRate())
			{
				_audio_sample_index = index;
				break;
			}
		}
	}

	_language = "und";
}

const std::shared_ptr<ov::Data> M4sInitWriter::CreateData()
{
	auto data_stream = std::make_shared<ov::Data>(4096);

	FtypBoxWrite(data_stream);
	MoovBoxWrite(data_stream);

	return data_stream;
}

int M4sInitWriter::FtypBoxWrite(std::shared_ptr<ov::Data> &data_stream)
{
	auto data = std::make_shared<ov::Data>();

	WriteText("mp42", data);			  // Major brand
	WriteUint32(0, data);				  // Minor version
	WriteText("isommp42iso5dash", data);  // Compatible brands // isom(4)mp42(4)iso5(4)dash(4)

	return WriteBoxData("ftyp", data, data_stream);
}

int M4sInitWriter::MoovBoxWrite(std::shared_ptr<ov::Data> &data_stream)
{
	auto data = std::make_shared<ov::Data>();

	MvhdBoxWrite(data);
	MvexBoxWrite(data);
	TrakBoxWrite(data);

	return WriteBoxData("moov", data, data_stream);
}

int M4sInitWriter::MvhdBoxWrite(std::shared_ptr<ov::Data> &data_stream)
{
	auto data = std::make_shared<ov::Data>();

	// 8.2.2.2 Syntax
	//
	// aligned(8) class MovieHeaderBox extends FullBox(‘mvhd’, version, 0) {
	//     if (version==1) {
	//         unsigned int(64) creation_time;
	//         unsigned int(64) modification_time;
	//         unsigned int(32) timescale;
	//         unsigned int(64) duration;
	//     } else { // version==0
	//         unsigned int(32) creation_time;
	//         unsigned int(32) modification_time;
	//         unsigned int(32) timescale;
	//         unsigned int(32) duration;
	//     }
	//     template int(32) rate = 0x00010000; // typically 1.0
	//     template int(16) volume = 0x0100; // typically, full volume
	//     const bit(16) reserved = 0;
	//     const unsigned int(32)[2] reserved = 0;
	//     template int(32)[9]  matrix =
	//         { 0x00010000,0,0,0,0x00010000,0,0,0,0x40000000 };
	//         // Unity matrix
	//     bit(32)[6] pre_defined = 0;
	//     unsigned int(32) next_track_ID;
	// }
	uint32_t matrix[9] =
		{0x00010000, 0, 0, 0, 0x00010000, 0, 0, 0, 0x40000000};

	WriteUint32(0x00000000, data);								   // creation_time
	WriteUint32(0x00000000, data);								   // modification_time
	WriteUint32(_main_track->GetTimeBase().GetTimescale(), data);  // timescale
	WriteUint32(0x00000000, data);								   // duration
	WriteUint32(0x00010000, data);								   // rate
	WriteUint16(0x0100, data);									   // volume
	WriteUint16(0x00000000, data);								   // reserved - bit(16)
	for (int i = 0; i < 2; i++)									   // reserved - int(32)[0]
	{
		WriteUint32(0x00000000, data);
	}
	for (int i = 0; i < static_cast<int>(OV_COUNTOF(matrix)); i++)  // matrix
	{
		WriteUint32(matrix[i], data);
	}
	for (int i = 0; i < 6; i++)  // pre_defined
	{
		WriteUint32(0x00000000, data);
	}
	WriteUint32(0XFFFFFFFF, data);  // Next Track ID

	return WriteBoxData("mvhd", 0, 0, data, data_stream);
}

int M4sInitWriter::TrakBoxWrite(std::shared_ptr<ov::Data> &data_stream)
{
	auto data = std::make_shared<ov::Data>();

	TkhdBoxWrite(data);
	MdiaBoxWrite(data);

	return WriteBoxData("trak", data, data_stream);
}

int M4sInitWriter::TkhdBoxWrite(std::shared_ptr<ov::Data> &data_stream)
{
	auto data = std::make_shared<ov::Data>();
	std::vector<uint8_t> metrix = {0, 0x01, 0, 0,
								   0, 0, 0, 0,
								   0, 0, 0, 0,
								   0, 0, 0, 0,
								   0, 0x01, 0, 0,
								   0, 0, 0, 0,
								   0, 0, 0, 0,
								   0, 0, 0, 0,
								   0x40, 0, 0, 0};

	WriteUint32(0, data);		   // Create Time
	WriteUint32(0, data);		   // Modification Time
	WriteUint32(_track_id, data);  // Track ID
	WriteInit(0, 4, data);		   // Reserve(4Byte)
	WriteUint32(_duration, data);  // Duration
	WriteInit(0, 8, data);		   // Reserve(8Byte)
	WriteUint16(0, data);		   // layer
	WriteUint16(0, data);		   // alternate group
	WriteUint16(0, data);		   // volume
	WriteInit(0, 2, data);		   // Reserve(2Byte)
	WriteData(metrix, data);	   // Matrix

	if (_media_type == M4sMediaType::Video)
	{
		WriteUint32(_video_track->GetWidth() << 16, data);   // Width
		WriteUint32(_video_track->GetHeight() << 16, data);  // Height
	}
	else
	{
		WriteUint32(0, data);  // Width
		WriteUint32(0, data);  // Height
	}

	return WriteBoxData("tkhd", 0, 7, data, data_stream);
}

int M4sInitWriter::MdiaBoxWrite(std::shared_ptr<ov::Data> &data_stream)
{
	auto data = std::make_shared<ov::Data>();

	MdhdBoxWrite(data);
	HdlrBoxWrite(data);
	MinfBoxWrite(data);

	return WriteBoxData("mdia", data, data_stream);
}

int M4sInitWriter::MdhdBoxWrite(std::shared_ptr<ov::Data> &data_stream)
{
	auto data = std::make_shared<ov::Data>();

	WriteUint32(0, data);																   // Create Time
	WriteUint32(0, data);																   // Modification Time
	WriteUint32(_main_track->GetTimeBase().GetTimescale(), data);						   // Timescale
	WriteUint32(0, data);																   // Duration
	WriteUint8((((_language[0] - 0x60) << 2) | (_language[1] - 0x60) >> 3) & 0xFF, data);  // Language 1
	WriteUint8((((_language[1] - 0x60) << 5) | (_language[2] - 0x60)) & 0xFF, data);	   // Language 2
	WriteUint16(0, data);																   // Pre Define

	return WriteBoxData("mdhd", 0, 0, data, data_stream);
}

int M4sInitWriter::HdlrBoxWrite(std::shared_ptr<ov::Data> &data_stream)
{
	auto data = std::make_shared<ov::Data>();

	WriteUint32(0, data);				// Pre Define
	WriteText(_handler_type, data);		// Handler Type
	WriteInit(0, 12, data);				// Reserve(12Byte)
	WriteText(_compressor_name, data);  // Handler Name
	WriteUint8(0, data);				// null

	return WriteBoxData("hdlr", 0, 0, data, data_stream);
}

int M4sInitWriter::MinfBoxWrite(std::shared_ptr<ov::Data> &data_stream)
{
	auto data = std::make_shared<ov::Data>();

	if (_media_type == M4sMediaType::Video)
	{
		VmhdBoxWrite(data);
	}
	else if (_media_type == M4sMediaType::Audio)
	{
		SmhdBoxWrite(data);
	}

	DinfBoxWrite(data);
	StblBoxWrite(data);

	return WriteBoxData("minf", data, data_stream);
}

int M4sInitWriter::VmhdBoxWrite(std::shared_ptr<ov::Data> &data_stream)
{
	auto data = std::make_shared<ov::Data>();

	WriteUint16(0, data);   // Graphics Mode
	WriteInit(0, 6, data);  // Op Color

	return WriteBoxData("vmhd", 0, 1, data, data_stream);
}

int M4sInitWriter::SmhdBoxWrite(std::shared_ptr<ov::Data> &data_stream)
{
	auto data = std::make_shared<ov::Data>();

	WriteUint16(0, data);  // Balance
	WriteUint16(0, data);  // Reserved

	return WriteBoxData("smhd", 0, 0, data, data_stream);
}

int M4sInitWriter::DinfBoxWrite(std::shared_ptr<ov::Data> &data_stream)
{
	auto data = std::make_shared<ov::Data>();

	DrefBoxWrite(data);

	return WriteBoxData("dinf", data, data_stream);
}

int M4sInitWriter::DrefBoxWrite(std::shared_ptr<ov::Data> &data_stream)
{
	auto data = std::make_shared<ov::Data>();

	WriteUint32(1, data);  // child count
	UrlBoxWrite(data);	 // url child

	return WriteBoxData("dref", 0, 0, data, data_stream);
}

int M4sInitWriter::UrlBoxWrite(std::shared_ptr<ov::Data> &data_stream)
{
	std::shared_ptr<ov::Data> data = nullptr;

	return WriteBoxData("url ", 0, 1, data, data_stream);
}

int M4sInitWriter::StblBoxWrite(std::shared_ptr<ov::Data> &data_stream)
{
	auto data = std::make_shared<ov::Data>();

	StsdBoxWrite(data);
	SttsBoxWrite(data);
	StscBoxWrite(data);
	StszBoxWrite(data);
	StcoBoxWrite(data);

	return WriteBoxData("stbl", data, data_stream);
}

int M4sInitWriter::StsdBoxWrite(std::shared_ptr<ov::Data> &data_stream)
{
	auto data = std::make_shared<ov::Data>();

	WriteUint32(1, data);  // Child Count

	if (_media_type == M4sMediaType::Video)
	{
		Avc1BoxWrite(data);
	}
	if (_media_type == M4sMediaType::Audio)
	{
		Mp4aBoxWrite(data);
	}

	return WriteBoxData("stsd", 0, 0, data, data_stream);
}

int M4sInitWriter::Avc1BoxWrite(std::shared_ptr<ov::Data> &data_stream)
{
	auto data = std::make_shared<ov::Data>();

	OV_ASSERT2(_video_track != nullptr);

	WriteUint32(1, data);									  // Child Count
	WriteUint16(0, data);									  // Pre Define
	WriteUint16(0, data);									  // Reserve(2Byte)
	WriteInit(0, 12, data);									  // Pre Define(12byte)
	WriteUint16((uint16_t)_video_track->GetWidth(), data);	// Width
	WriteUint16((uint16_t)_video_track->GetHeight(), data);   // Height
	WriteUint32(0x00480000, data);							  // Horiz Resolution
	WriteUint32(0x00480000, data);							  // Vert Resolution
	WriteUint32(0, data);									  // Reserve(4Byte)
	WriteUint16(1, data);									  // Frame Count
	WriteUint8((uint8_t)_compressor_name.GetLength(), data);  // Compressor Name Size(Max 31Byte)
	WriteText(_compressor_name, data);						  // Compressor Name
	WriteInit(0, 31 - _compressor_name.GetLength(), data);	// Padding(31 - Compressor Name Size)
	WriteUint16(0x0018, data);								  // Depth
	WriteUint16(0xFFFF, data);								  // Pre Define

	AvccBoxWrite(data);

	return WriteBoxData("avc1", 0, 0, data, data_stream);
}

int M4sInitWriter::AvccBoxWrite(std::shared_ptr<ov::Data> &data_stream)
{
	auto data = std::make_shared<ov::Data>();

	uint8_t avc_profile = 0;
	uint8_t avc_profile_compatibility = 0;
	uint8_t avc_level = 0;
	uint8_t avc_nal_unit_size = 4;

	if (_avc_sps->GetLength() >= 4)
	{
		auto buffer = _avc_sps->GetDataAs<uint8_t>();

		avc_profile = buffer[1];
		avc_profile_compatibility = buffer[2];
		avc_level = buffer[3];
	}

	WriteUint8(1, data);										  // Configuration Version
	WriteUint8(avc_profile, data);								  // Profile
	WriteUint8(avc_profile_compatibility, data);				  // Profile Compatibillity
	WriteUint8(avc_level, data);								  // Level
	WriteUint8((uint8_t)((avc_nal_unit_size - 1) | 0xFC), data);  // Nal Unit Size
	WriteUint8(1 | 0xE0, data);									  // SPS Count
	WriteUint16(_avc_sps->GetLength(), data);					  // SPS Size
	WriteData(_avc_sps, data);									  // SPS
	WriteUint8(1, data);										  // PPS Count
	WriteUint16(_avc_pps->GetLength(), data);					  // PPS Size
	WriteData(_avc_pps, data);									  // PPS

	return WriteBoxData("avcC", data, data_stream);
}

int M4sInitWriter::Mp4aBoxWrite(std::shared_ptr<ov::Data> &data_stream)
{
	auto data = std::make_shared<ov::Data>();

	OV_ASSERT2(_audio_track != nullptr);

	WriteUint32(1, data);											   // Child Count
	WriteUint16(0, data);											   // QT version
	WriteUint16(0, data);											   // QT revision
	WriteUint32(0, data);											   // QT vendor
	WriteUint16(_audio_track->GetChannel().GetCounts(), data);		   // channel count
	WriteUint16(_audio_track->GetSample().GetSampleSize() * 8, data);  // sample size
	WriteUint16(0, data);											   // QT compression ID
	WriteUint16(0, data);											   // QT packet size
	WriteUint32(_audio_track->GetSampleRate() << 16, data);			   // sample rate

	EsdsBoxWrite(data);

	return WriteBoxData("mp4a", 0, 0, data, data_stream);
}

int M4sInitWriter::EsdsBoxWrite(std::shared_ptr<ov::Data> &data_stream)
{
	auto data = std::make_shared<ov::Data>();

	// es id(3)
	WriteUint8(3, data);	 // tag
	WriteUint8(0x19, data);  // tag size
	WriteUint16(0, data);	// es id ??? track id
	WriteUint8(0, data);	 // flag

	// decoder config(13)
	WriteUint8(4, data);	 // tag
	WriteUint8(0x11, data);  // tag size
	WriteUint8(0x40, data);  // Object type indication  - MPEG-4 audio (0X40)
	WriteUint8(0x15, data);  // Stream type( <<2)  / Up Stream ( << 1) / Reserve(0x01)
	WriteUint24(0, data);	// Buffer Size
	WriteUint32(0, data);	// MaxBitrate
	WriteUint32(0, data);	// AverageBitreate

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
	bit_writer.Write(5, 2);										  // object type - 2: AAC LC (Low Complexity)
	bit_writer.Write(4, _audio_sample_index);					  // frequency index
	bit_writer.Write(4, _audio_track->GetChannel().GetCounts());  // channel configuration

	WriteUint8(5, data);  // tag
	WriteUint8(2, data);  // tag size

	WriteData(bit_writer.GetData(), (int)bit_writer.GetDataSize(), data);  //

	// sl config(1)
	WriteUint8(6, data);  // tag
	WriteUint8(1, data);  // tag size
	WriteUint8(2, data);  // always 2 refer from mov_write_esds_tag

	return WriteBoxData("esds", 0, 0, data, data_stream);
}

int M4sInitWriter::SttsBoxWrite(std::shared_ptr<ov::Data> &data_stream)
{
	auto data = std::make_shared<ov::Data>();

	WriteUint32(0, data);  // Entry Count

	return WriteBoxData("stts", 0, 0, data, data_stream);
}

int M4sInitWriter::StscBoxWrite(std::shared_ptr<ov::Data> &data_stream)
{
	auto data = std::make_shared<ov::Data>();

	WriteUint32(0, data);  // Entry Count

	return WriteBoxData("stsc", 0, 0, data, data_stream);
}

int M4sInitWriter::StszBoxWrite(std::shared_ptr<ov::Data> &data_stream)
{
	auto data = std::make_shared<ov::Data>();

	WriteUint32(0, data);  // Sample Size
	WriteUint32(0, data);  // Sample Count

	return WriteBoxData("stsz", 0, 0, data, data_stream);
}

int M4sInitWriter::StcoBoxWrite(std::shared_ptr<ov::Data> &data_stream)
{
	auto data = std::make_shared<ov::Data>();

	WriteUint32(0, data);  // Entry Count

	return WriteBoxData("stco", 0, 0, data, data_stream);
}

int M4sInitWriter::MvexBoxWrite(std::shared_ptr<ov::Data> &data_stream)
{
	auto data = std::make_shared<ov::Data>();

	//MehdBoxWrite(data);
	TrexBoxWrite(data);

	return WriteBoxData("mvex", data, data_stream);
}

int M4sInitWriter::MehdBoxWrite(std::shared_ptr<ov::Data> &data_stream)
{
	auto data = std::make_shared<ov::Data>();

	return WriteBoxData("mehd", 0, 0, data, data_stream);
}

int M4sInitWriter::TrexBoxWrite(std::shared_ptr<ov::Data> &data_stream)
{
	auto data = std::make_shared<ov::Data>();

	WriteUint32(_track_id, data);  // Track ID
	WriteUint32(1, data);		   // Sample Description Index
	WriteUint32(1, data);		   // Sample Duration
	WriteUint32(1, data);		   // Sample Size
	WriteUint32(0, data);		   // Sample Flags

	return WriteBoxData("trex", 0, 0, data, data_stream);
}
