//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Getroot
//  Copyright (c) 2022 AirenSoft. All rights reserved.
//
//==============================================================================

#include "bmff_packager.h"
#include "bmff_private.h"

namespace bmff
{
	Packager::Packager(const std::shared_ptr<const MediaTrack> &track)
	{
		_track = track;
	}

	// Get track 
	const std::shared_ptr<const MediaTrack> &Packager::GetTrack() const
	{
		return _track;
	}

	bool Packager::WriteFtypBox(ov::ByteStream &container_stream)
	{
		// ISO/IEC 14496-12 4.3
		// aligned(8) class FileTypeBox	extends Box(‘ftyp’)
		// {
		// 	unsigned int(32) major_brand;
		// 	unsigned int(32) minor_version;
		// 	unsigned int(32) compatible_brands[];  // to end of the box
		// }

		ov::ByteStream stream(128);

		stream.WriteText("iso6"); // major brand
		stream.WriteBE32(0); // minor version
		stream.WriteText("iso6mp42avc1dashhlsf"); // compatible brands
		
		return WriteBox(container_stream, "ftyp", *stream.GetData());
	}

	bool Packager::WriteMoovBox(ov::ByteStream &container_stream)
	{
		// ISO/IEC 14496-12 8.2.1
		// aligned(8) class MovieBox extends Box(‘moov’)
		// {
		// }

		ov::ByteStream stream(1024);

		if (WriteMvhdBox(stream) == false)
		{
			logte("Packager::WriteMoovBox() - Failed to write mvhd box");
			return false;
		}

		if (WriteTrakBox(stream) == false)
		{
			logte("Packager::WriteMoovBox() - Failed to write trak box");
			return false;
		}

		if (WriteMvexBox(stream) == false)
		{
			logte("Packager::WriteMoovBox() - Failed to write mvex box");
			return false;
		}

		return WriteBox(container_stream, "moov", *stream.GetData());
	}

	bool Packager::WriteMvhdBox(ov::ByteStream &container_stream)
	{
		// ISO/IEC 14496-12 8.2.2
		// aligned(8) class MovieHeaderBox extends FullBox(‘mvhd’, version, 0)
		// {
		// 	if (version == 1)
		// 	{
		// 		unsigned int(64) creation_time;
		// 		unsigned int(64) modification_time;
		// 		unsigned int(32) timescale;
		// 		unsigned int(64) duration;
		// 	}
		// 	else
		// 	{  // version==0
		// 		unsigned int(32) creation_time;
		// 		unsigned int(32) modification_time;
		// 		unsigned int(32) timescale;
		// 		unsigned int(32) duration;
		// 	}
		// 	template int(32) rate = 0x00010000;	 // typically 1.0
		// 	template int(16) volume = 0x0100;	 // typically, full volume
		// 	const bit(16) reserved = 0;
		// 	const unsigned int(32)[2] reserved = 0;
		// 	template int(32)[9] matrix =
		// 		{0x00010000, 0, 0, 0, 0x00010000, 0, 0, 0, 0x40000000};
		// 	// Unity matrix
		// 	bit(32)[6] pre_defined = 0;
		// 	unsigned int(32) next_track_ID;
		// }

		ov::ByteStream stream(128);
		uint32_t matix[9] = { 0x00010000, 0, 0, 0, 0x00010000, 0, 0, 0, 0x40000000 };

		stream.WriteBE32(0); // creation_time
		stream.WriteBE32(0); // modification_time
		stream.WriteBE32(GetTrack()->GetTimeBase().GetTimescale()); // timescale
		stream.WriteBE32(0); // duration
		stream.WriteBE32(0x00010000); // rate
		stream.WriteBE16(0x0100); // volume
		stream.WriteBE16(0); // reserved
		stream.WriteBE32(0); // reserved
		stream.WriteBE32(0); // reserved
		// Matrix
		for (int i=0; i<9; i++)
		{
			stream.WriteBE32(matix[i]);
		}
		// Pre-defined
		for (int i=0; i<6; i++)
		{
			stream.WriteBE32(0);
		}

		// Next track ID
		stream.WriteBE32(0xFFFFFFFF);

		return WriteFullBox(container_stream, "mvhd", *stream.GetData(), 0, 0);
	}

	bool Packager::WriteTrakBox(ov::ByteStream &container_stream)
	{
		// ISO/IEC 14496-12 8.3.1
		// aligned(8) class TrackBox extends Box(‘trak’)
		// {
		// }

		ov::ByteStream stream(4096);

		if (WriteTkhdBox(stream) == false)
		{
			logte("Packager::WriteTrakBox() - Failed to write tkhd box");
			return false;
		}

		if (WriteMdiaBox(stream) == false)
		{
			logte("Packager::WriteTrakBox() - Failed to write mdia box");
			return false;
		}

		return WriteBox(container_stream, "trak", *stream.GetData());
	}

	bool Packager::WriteTkhdBox(ov::ByteStream &container_stream)
	{
		// ISO/IEC 14496-12 8.3.2
		// aligned(8) class TrackHeaderBox extends FullBox(‘tkhd’, version, flags)
		// {
		// 	if (version == 1)
		// 	{
		// 		unsigned int(64) creation_time;
		// 		unsigned int(64) modification_time;
		// 		unsigned int(32) track_ID;
		// 		const unsigned int(32) reserved = 0;
		// 		unsigned int(64) duration;
		// 	}
		// 	else
		// 	{  // version==0
		// 		unsigned int(32) creation_time;
		// 		unsigned int(32) modification_time;
		// 		unsigned int(32) track_ID;
		// 		const unsigned int(32) reserved = 0;
		// 		unsigned int(32) duration;
		// 	}
		// 	const unsigned int(32)[2] reserved = 0;
		// 	template int(16) layer = 0;
		// 	template int(16) alternate_group = 0;
		// 	template int(16) volume = { if track_is_audio 0x0100 else 0 };
		// 	const unsigned int(16) reserved = 0;
		// 	template int(32)[9] matrix =
		// 		{0x00010000, 0, 0, 0, 0x00010000, 0, 0, 0, 0x40000000};
		// 	// unity matrix
		// 	unsigned int(32) width;
		// 	unsigned int(32) height;
		// }

		ov::ByteStream stream(128);
		uint32_t matix[9] = { 0x00010000, 0, 0, 0, 0x00010000, 0, 0, 0, 0x40000000 };

		stream.WriteBE32(0); // creation_time
		stream.WriteBE32(0); // modification_time

		// track_ID is an integer that uniquely identifies this track over the entire life‐time of this presentation. Track IDs are never re-used and cannot be zero.
		// track_ID + 1 because zero track ID is valid for OME
		stream.WriteBE32(GetTrack()->GetId()+1); // track_ID
		stream.WriteBE32(0); // reserved
		stream.WriteBE32(0); // duration
		stream.WriteBE32(0); // reserved
		stream.WriteBE32(0); // reserved
		stream.WriteBE16(0); // layer
		stream.WriteBE16(0); // alternate_group

		if (GetTrack()->GetMediaType() == cmn::MediaType::Audio)
		{
			stream.WriteBE16(0x0100); // volume
		}
		else
		{
			stream.WriteBE16(0); // volume
		}

		stream.WriteBE16(0); // reserved
		// Matrix
		for (int i=0; i<9; i++)
		{
			stream.WriteBE32(matix[i]);
		}

		// Width and height
		// [ISO/IEC 14496-12 8.5.3] specify the track's visual presentation size as fixed-point 16.16 values. 
		stream.WriteBE32(_track->GetWidth() << 16);
		stream.WriteBE32(_track->GetHeight() << 16);

		//[ISO/IEC 14496-12 8.5.1] The default value of the track header flags for media tracks is 7 (track_enabled, track_in_movie, track_in_preview). 
		return WriteFullBox(container_stream, "tkhd", *stream.GetData(), 0, 7);
	}

	bool Packager::WriteMdiaBox(ov::ByteStream &container_stream)
	{
		// ISO/IEC 14496-12 8.4.1
		// aligned(8) class MediaBox extends Box(‘mdia’)
		// {
		// 	MediaHeaderBox mdhd;
		// 	HandlerBox hdlr;
		// 	MediaInformationBox minf;
		// }

		ov::ByteStream stream(4096);
		if (WriteMdhdBox(stream) == false)
		{
			logte("Packager::WriteMdiaBox() - Failed to write mdhd box");
			return false;
		}

		if (WriteHdlrBox(stream) == false)
		{
			logte("Packager::WriteMdiaBox() - Failed to write hdlr box");
			return false;
		}

		if (WriteMinfBox(stream) == false)
		{
			logte("Packager::WriteMdiaBox() - Failed to write minf box");
			return false;
		}

		return WriteBox(container_stream, "mdia", *stream.GetData());
	}

	bool Packager::WriteMdhdBox(ov::ByteStream &container_stream)
	{
		// ISO/IEC 14496-12 8.4.2
		// aligned(8) class MediaHeaderBox extends FullBox(‘mdhd’, version, 0)
		// {
		// 	if (version == 1)
		// 	{
		// 		unsigned int(64) creation_time;
		// 		unsigned int(64) modification_time;
		// 		unsigned int(32) timescale;
		// 		unsigned int(64) duration;
		// 	}
		// 	else
		// 	{  // version==0
		// 		unsigned int(32) creation_time;
		// 		unsigned int(32) modification_time;
		// 		unsigned int(32) timescale;
		// 		unsigned int(32) duration;
		// 	}
		// 	bit(1) pad = 0;
		// 	unsigned int(5)[3] language;  // ISO-639-2/T language code
		// 	unsigned int(16) pre_defined = 0;
		// }

		ov::ByteStream stream(128);
		char language[3] = { 'u', 'n', 'd' };

		stream.WriteBE32(0); // creation_time
		stream.WriteBE32(0); // modification_time
		stream.WriteBE32(_track->GetTimeBase().GetTimescale()); // timescale
		stream.WriteBE32(0); // duration

		// language
		// [ISO/IEC 14496-12 8.8.3] 
		// declares the language code for this media. See ISO 639-2/T for the set of three character
		// codes. Each character is packed as the difference between its ASCII value and 0x60. Since the code
		// is confined to being three lower-case letters, these values are strictly positive. 
		char lang1 = language[0] - 0x60;
		char lang2 = language[1] - 0x60;
		char lang3 = language[2] - 0x60;

		stream.Write8((((lang1 << 2) & 0b01111100) | ((lang2 >> 3) & 0b11)) & 0xFF);
		stream.Write8((((lang2 & 0b111) << 5) | (lang3 & 0b11111)) & 0xFF);

		stream.WriteBE16(0); // pre_defined

		return WriteFullBox(container_stream, "mdhd", *stream.GetData(), 0, 0);
	}

	bool Packager::WriteHdlrBox(ov::ByteStream &container_stream)
	{
		// ISO/IEC 14496-12 8.4.3
		// aligned(8) class HandlerBox extends FullBox(‘hdlr’, version = 0, 0)
		// {
		// 	unsigned int(32) pre_defined = 0;
		// 	unsigned int(32) handler_type;
		// 	const unsigned int(32)[3] reserved = 0;
		// 	string name;
		// }

		ov::ByteStream stream(128);
		
		stream.WriteBE32(0); // pre_defined

		// handler_type
		if (GetTrack()->GetMediaType() == cmn::MediaType::Video)
		{
			stream.WriteText("vide");
		}
		else if (GetTrack()->GetMediaType() == cmn::MediaType::Audio)
		{
			stream.WriteText("soun");
		}
		else
		{
			stream.WriteText("hint");
		}

		for (int i=0; i<3; i++)
		{
			stream.WriteBE32(0); // reserved
		}

		// name
		// [ISO/IEC 14496-12 8.9.3] 
		// name is a null-terminated string in UTF-8 characters which gives a human-readable name for the track
		// type (for debugging and inspection purposes). 		
		stream.WriteText(StringFromMediaType(GetTrack()->GetMediaType()), true);

		return WriteFullBox(container_stream, "hdlr", *stream.GetData(), 0, 0);
	}

	bool Packager::WriteMinfBox(ov::ByteStream &container_stream)
	{
		// ISO/IEC 14496-12 8.4.4
		// aligned(8) class MediaInformationBox extends Box(‘minf’)
		// {
		// 	if (type == ‘vide’)
		// 	{
		// 		VideoMediaHeaderBox vmhd;
		// 	}
		// 	else if (type == ‘soun’)
		// 	{
		// 		SoundMediaHeaderBox smhd;
		// 	}
		// 	else
		// 	{
		// 		NullMediaHeaderBox nmhd;
		// 	}
		// 
		// 	DataInformationBox dinf;
		// 	SampleTableBox stbl;
		// }

		ov::ByteStream stream(4096);
		if (GetTrack()->GetMediaType() == cmn::MediaType::Video)
		{
			if (WriteVmhdBox(stream) == false)
			{
				logte("Packager::WriteMinfBox() - Failed to write vmhd box");
				return false;
			}
		}
		else if (GetTrack()->GetMediaType() == cmn::MediaType::Audio)
		{
			if (WriteSmhdBox(stream) == false)
			{
				logte("Packager::WriteMinfBox() - Failed to write smhd box");
				return false;
			}
		}
		else
		{
			logte("Packager::WriteMinfBox() - Unsupported media type");
			return false;
			// Not supported yet
			// WriteNmhdBox(stream);
		}

		// Dinf
		if (WriteDinfBox(stream) == false)
		{
			logte("Packager::WriteMinfBox() - Failed to write dinf box");
			return false;
		}

		// Stbl
		if (WriteStblBox(stream) == false)
		{
			logte("Packager::WriteMinfBox() - Failed to write stbl box");
			return false;
		}

		return WriteBox(container_stream, "minf", *stream.GetData());
	}

	bool Packager::WriteVmhdBox(ov::ByteStream &container_stream)
	{
		// ISO/IEC 14496-12 12.1.2
		// aligned(8) class VideoMediaHeaderBox	extends FullBox(‘vmhd’, version = 0, 1)
		// {
		// 	template unsigned int(16) graphicsmode = 0;	 // copy, see below
		// 	template unsigned int(16)[3] opcolor = {0, 0, 0};
		// }

		ov::ByteStream stream(128);
		
		stream.WriteBE16(0); // graphicsmode
		stream.WriteBE16(0); // opcolor[0]
		stream.WriteBE16(0); // opcolor[1]
		stream.WriteBE16(0); // opcolor[2]
		
		// [ISO/IEC 14496-12 8.11.2]
		// The video media header contains general presentation information, independent of the coding, for video
		// media. Note that the flags field has the value 1.
		return WriteFullBox(container_stream, "vmhd", *stream.GetData(), 0, 1);
	}

	bool Packager::WriteSmhdBox(ov::ByteStream &container_stream)
	{
		// ISO/IEC 14496-12 12.2.2
		// aligned(8) class SoundMediaHeaderBox	extends FullBox(‘smhd’, version = 0, 0)
		// {
		// 	template int(16) balance = 0;
		// 	const unsigned int(16) reserved = 0;
		// }

		ov::ByteStream stream(128);

		// [ISO/IEC 14496-12 8.11.3.2]
		// balance is a fixed-point 8.8 number that places mono audio tracks in a stereo space; 0 is center (the
		// normal value); full left is -1.0 and full right is 1.0.
		stream.WriteBE16(0); // balance
		stream.WriteBE16(0); // reserved

		return WriteFullBox(container_stream, "smhd", *stream.GetData(), 0, 0);
	}

	bool Packager::WriteDinfBox(ov::ByteStream &container_stream)
	{
		// ISO/IEC 14496-12 8.7.1
		// aligned(8) class DataInformationBox extends Box(‘dinf’)
		// {
		// 	DataReferenceBox dref; // required
		// }

		ov::ByteStream stream(4096);
		// Dref
		if (WriteDrefBox(stream) == false)
		{
			logte("Packager::WriteDinfBox() - Failed to write dref box");
			return false;
		}

		return WriteBox(container_stream, "dinf", *stream.GetData());
	}

	bool Packager::WriteDrefBox(ov::ByteStream &container_stream)
	{
		// ISO/IEC 14496-12 8.7.2
		// aligned(8) class DataReferenceBox
		// 	extends FullBox(‘dref’, version = 0, 0)
		// {
		// 	unsigned int(32) entry_count;
		// 	for (i = 1; i • entry_count; i++)
		// 	{
		// 		entry_count; i++)
		// 		{
		// 			DataEntryBox(entry_version, entry_flags) data_entry;
		// 		}
		// 	}

		ov::ByteStream stream(4096);

		stream.WriteBE32(1); // entry_count
		if (WriteUrlBox(stream) == false)
		{
			logte("Packager::WriteDrefBox() - Failed to write url box");
			return false;
		}

		return WriteFullBox(container_stream, "dref", *stream.GetData(), 0, 0);
	}

	bool Packager::WriteUrlBox(ov::ByteStream &container_stream)
	{
		// ISO/IEC 14496-12 8.7.2
		// aligned(8) class DataEntryUrlBox(bit(24) flags) extends FullBox(‘url ’, version = 0, flags)
		// {
		// 	string location;
		// }

		ov::ByteStream stream(128);

		//stream.WriteText("https://www.ovenmediaengine.com/", true);

		return WriteFullBox(container_stream, "url ", *stream.GetData(), 0, 1);
	}

	bool Packager::WriteStblBox(ov::ByteStream &container_stream)
	{
		// ISO/IEC 14496-12 8.5.1
		// aligned(8) class SampleTableBox extends Box(‘stbl’)
		// {
		// 	SampleTableBox stsd;
		// 	TimeToSampleBox stts;
		//	SampleToChunkBox stsc;
		//	ChunkOffsetBox stco;
		//	SampleSizeBox stsz; // Optional
		// }

		ov::ByteStream stream(4096);
		// Stsd
		if (WriteStsdBox(stream) == false)
		{
			logte("Packager::WriteStblBox() - Failed to write stsd box");
			return false;
		}
		// Stts
		if (WriteSttsBox(stream) == false)
		{
			logte("Packager::WriteStblBox() - Failed to write stts box");
			return false;
		}
		// Stsc
		if (WriteStscBox(stream) == false)
		{
			logte("Packager::WriteStblBox() - Failed to write stsc box");
			return false;
		}
		// Stsz
		if (WriteStszBox(stream) == false)
		{
			logte("Packager::WriteStblBox() - Failed to write stsz box");
			return false;
		}
		// Stco
		if (WriteStcoBox(stream) == false)
		{
			logte("Packager::WriteStblBox() - Failed to write stco box");
			return false;
		}

		return WriteBox(container_stream, "stbl", *stream.GetData());
	}

	bool Packager::WriteStsdBox(ov::ByteStream &container_stream)
	{
		// ISO/IEC 14496-12 8.5.2
		// aligned(8) class SampleDescriptionBox(unsigned int(32) handler_type)	extends FullBox('stsd', 0, 0)
		// {
		// 	int i;
		// 	unsigned int(32) entry_count;
		// 	for (i = 1; i u entry_count; i++)
		// 	{
		// 		switch (handler_type)
		// 		{
		// 			case ‘soun’:  // for audio tracks
		// 				AudioSampleEntry();
		// 				break;
		// 			case ‘vide’:  // for video tracks
		// 				VisualSampleEntry();
		// 				break;
		// 			case ‘hint’:  // Hint track
		// 				HintSampleEntry();
		// 				break;
		// 		}
		// 	}
		// }

		ov::ByteStream stream(4096);

		// entry_count
		stream.WriteBE32(1);

		if (GetTrack()->GetCodecId() == cmn::MediaCodecId::H264)
		{
			if (WriteAvc1Box(stream) == false)
			{
				logte("Packager::WriteStsdBox() - Failed to write avc1 box");
				return false;
			}
		}
		else if (GetTrack()->GetCodecId() == cmn::MediaCodecId::Aac)
		{
			if (WriteMp4aBox(stream) == false)
			{
				logte("Packager::WriteStsdBox() - Failed to write mp4a box");
				return false;
			}
		}
		else
		{
			// Not yet supported
			logte("Packager::WriteStsdBox() - Unsupported codec id");
			return false;
		}

		return WriteFullBox(container_stream, "stsd", *stream.GetData(), 0, 0);
	}

	bool Packager::WriteAvc1Box(ov::ByteStream &container_stream)
	{
		// ISO/IEC 14496-12 8.5.2.2
		// aligned(8) abstract class SampleEntry(unsigned int(32) format) extends Box(format)
		// {
		// 	const unsigned int(8)[6] reserved = 0;
		// 	unsigned int(16) data_reference_index;
		// }

		// ISO/IEC 14496-12 12.1.3
		// class VisualSampleEntry(codingname) extends SampleEntry(codingname)
		// {
		// 	unsigned int(16) pre_defined = 0;
		// 	const unsigned int(16) reserved = 0;
		// 	unsigned int(32)[3] pre_defined = 0;
		// 	unsigned int(16) width;
		// 	unsigned int(16) height;
		// 	template unsigned int(32) horizresolution = 0x00480000;	 // 72 dpi
		// 	template unsigned int(32) vertresolution = 0x00480000;	 // 72 dpi
		// 	const unsigned int(32) reserved = 0;
		// 	template unsigned int(16) frame_count = 1;
		// 	string[32] compressorname;
		// 	template unsigned int(16) depth = 0x0018;
		// 	int(16) pre_defined = -1;
		// }

		// ISO/IEC 14496-15 5.3.4.1
		// class AVCSampleEntry() extends VisualSampleEntry(‘avc1’)
		// {
		// 	AVCConfigurationBox config;
		// 	MPEG4BitRateBox();				 // optional
		// 	MPEG4ExtensionDescriptorsBox();	 // optional
		// }

		// ISO/IEC 14496-15 5.3.4.1
		// class AVCConfigurationBox extends Box(‘avcC’)
		// {
		// 	AVCDecoderConfigurationRecord() AVCConfig;
		// }

		ov::ByteStream stream(4096);

		// Reserved int(8)[6]
		for (int i=0; i<6; i++)
		{
			stream.Write8(0);
		}

		// Data reference index
		stream.WriteBE16(1);
		// pre_defined int(16)
		stream.WriteBE16(0);
		// reserved int(16)
		stream.WriteBE16(0);
		// pre_defined int(32)[3]
		for (int i=0; i<3; i++)
		{
			stream.WriteBE32(0);
		}

		// Width int(16)
		stream.WriteBE16(GetTrack()->GetWidth());
		// Height int(16)
		stream.WriteBE16(GetTrack()->GetHeight());
		// Horizontal resolution int(32)
		stream.WriteBE32(0x00480000);
		// Vertical resolution int(32)
		stream.WriteBE32(0x00480000);

		// Reserved int(32)
		stream.WriteBE32(0);

		// Frame count int(16)
		// frame_count indicates how many frames of compressed video are stored in each sample. The default is
		// 1, for one frame per sample; it may be more than 1 for multiple frames per sample 
		stream.WriteBE16(1);

		// Compressorname string[32]
		// Compressorname is a name, for informative purposes. It is formatted in a fixed 32-byte field, with the
		// first byte set to the number of bytes to be displayed, followed by that number of bytes of displayable
		// data, and then padding to complete 32 bytes total (including the size byte). The field may be set to 0.
		ov::String compressorname = "OvenMediaEngine";
		stream.Write8(compressorname.GetLength()); // Size
		stream.WriteText(compressorname); // Data (max : 31 bytes)
		for (size_t i=0; i<(31 - compressorname.GetLength()); i++) // Padding
		{
			stream.Write8(0);
		}

		// Depth int(16)
		// depth indicates the number of bits per pixel. The default value is 0x0018, indicating 24 bits per pixel.
		// 0x0018 – images are in colour with no alpha
		stream.WriteBE16(0x0018);

		// Pre-defined int(16)
		// pre_defined is a 16-bit integer that must be set to -1.
		stream.WriteBE16(-1);

		if (WriteAvccBox(stream) == false)
		{
			logte("Packager::WriteAvc1Box() - Failed to write avcc box");
			return false;
		}

		return WriteBox(container_stream, "avc1", *stream.GetData());
	}

	bool Packager::WriteAvccBox(ov::ByteStream &container_stream)
	{
		// ISO/IEC 14496-15 5.3.4.1
		// class AVCConfigurationBox extends Box(‘avcC’)
		// {
		// 	AVCDecoderConfigurationRecord() AVCConfig;
		// }

		// ISO/IEC 14496-15 5.2.4.1
		// aligned(8) class AVCDecoderConfigurationRecord
		// {
		// 	unsigned int(8) configurationVersion = 1;
		// 	unsigned int(8) AVCProfileIndication;
		// 	unsigned int(8) profile_compatibility;
		// 	unsigned int(8) AVCLevelIndication;
		// 	bit(6) reserved = ‘111111’b;
		// 	unsigned int(2) lengthSizeMinusOne;
		// 	bit(3) reserved = ‘111’b;
		// 	unsigned int(5) numOfSequenceParameterSets;
		// 	for (i = 0; i < numOfSequenceParameterSets; i++)
		// 	{
		// 		unsigned int(16) sequenceParameterSetLength;
		// 		bit(8 * sequenceParameterSetLength) sequenceParameterSetNALUnit;
		// 	}
		// 	unsigned int(8) numOfPictureParameterSets;
		// 	for (i = 0; i < numOfPictureParameterSets; i++)
		// 	{
		// 		unsigned int(16) pictureParameterSetLength;
		// 		bit(8 * pictureParameterSetLength) pictureParameterSetNALUnit;
		// 	}
		// 	if (profile_idc == 100 || profile_idc == 110 ||
		// 		profile_idc == 122 || profile_idc == 144)
		// 	{
		// 		bit(6) reserved = ‘111111’b;
		// 		unsigned int(2) chroma_format;
		// 		bit(5) reserved = ‘11111’b;
		// 		unsigned int(3) bit_depth_luma_minus8;
		// 		bit(5) reserved = ‘11111’b;
		// 		unsigned int(3) bit_depth_chroma_minus8;
		// 		unsigned int(8) numOfSequenceParameterSetExt;
		// 		for (i = 0; i < numOfSequenceParameterSetExt; i++)
		// 		{
		// 			unsigned int(16) sequenceParameterSetExtLength;
		// 			bit(8 * sequenceParameterSetExtLength) sequenceParameterSetExtNALUnit;
		// 		}
		// 	}
		// }

		ov::ByteStream stream(4096);

		// configurationVersion int(8)
		stream.Write8(1);

		auto sps_data = GetTrack()->GetH264SpsData();
		auto pps_data = GetTrack()->GetH264PpsData();

		if (sps_data == nullptr || pps_data == nullptr)
		{
			logte("Packager::WriteAvccBox() - Failed to get sps/pps data");
			return false;
		}

		// Parse SPS
		H264SPS sps;
		if (H264Parser::ParseSPS(sps_data->GetDataAs<uint8_t>(), sps_data->GetLength(), sps) == false)
		{
			logte("Could not parse H264 SPS unit");
			return false;
		}

		// AVCProfileIndication int(8)
		stream.Write8(sps.GetProfileIdc());

		// profile_compatibility int(8)
		stream.Write8(sps.GetConstraintFlag());

		// AVCLevelIndication int(8)
		stream.Write8(sps.GetCodecLevelIdc());

		// lengthSizeMinusOne int(2)
		// lengthSizeMinusOne indicates the length in bytes of the NALUnitLength field in an AVC video
		// sample or AVC parameter set sample of the associated stream minus one
		
		// reserved bit(6) '111111' | lengthSizeMinusOne bit(2) '11'
		stream.Write8(0xFC | 3);

		// reserved bit(3) '111' | numOfSequenceParameterSets int(5)
		stream.Write8(0xE0 | 1);

		// sequenceParameterSetLength
		stream.WriteBE16(sps_data->GetLength());

		// sequenceParameterSetNALUnit
		stream.Write(sps_data);

		// numOfPictureParameterSets int(8)
		stream.Write8(1);

		// pictureParameterSetLength
		stream.WriteBE16(pps_data->GetLength());

		// pictureParameterSetNALUnit
		stream.Write(pps_data);
		
		return WriteBox(container_stream, "avcC", *stream.GetData());
	}

	bool Packager::WriteMp4aBox(ov::ByteStream &container_stream)
	{
		// ISO/IEC 14496-12 8.5.2.2
		// aligned(8) abstract class SampleEntry(unsigned int(32) format) extends Box(format)
		// {
		// 	const unsigned int(8)[6] reserved = 0;
		// 	unsigned int(16) data_reference_index;
		// }

		// ISO/IEC 14496-12 12.2.3.2 
		// class AudioSampleEntry(codingname) extends SampleEntry(codingname)
		// {
		// 	const unsigned int(32)[2] reserved = 0;
		// 	template unsigned int(16) channelcount = 2;
		// 	template unsigned int(16) samplesize = 16;
		// 	unsigned int(16) pre_defined = 0;
		// 	const unsigned int(16) reserved = 0;
		// 	template unsigned int(32) samplerate = {default samplerate of media} << 16;

		// optional boxes follow
		// 	ChannelLayout();
		// 	// we permit any number of DownMix or DRC boxes:
		// 	DownMixInstructions()[];
		// 	DRCCoefficientsBasic()[];
		// 	DRCInstructionsBasic()[];
		// 	DRCCoefficientsUniDRC()[];
		// 	DRCInstructionsUniDRC()[];
		// 	Box();	// further boxes as needed
		// }

		// ISO/IEC 14496-14 5.6.1
		// class MP4AudioSampleEntry() extends AudioSampleEntry('mp4a')
		// {
		// 	ESDBox ES;
		// }

		ov::ByteStream stream(4096);

		// uint(8)[6] reserved = 0;
		for (int i = 0; i < 6; i++)
		{
			stream.Write8(0);
		}

		// uint(16) data_reference_index;
		stream.WriteBE16(1); // nl

		// uint(32)[2] reserved = 0;
		for (int i = 0; i < 2; i++)
		{
			stream.WriteBE32(0); 
		}

		// uint(16) channelcount = 2;
		stream.WriteBE16(2);
		// uint(16) samplesize = 16;
		stream.WriteBE16(16);
		// uint(16) pre_defined = 0;
		stream.WriteBE16(0);
		// uint(16) reserved = 0;
		stream.WriteBE16(0); // nl
		// uint(32) samplerate = {default samplerate of media} << 16;
		stream.WriteBE32(GetTrack()->GetSampleRate() << 16);

		if (WriteEsdsBox(stream) == false)
		{
			return false;
		}
		
		return WriteBox(container_stream, "mp4a", *stream.GetData());
	}

	bool Packager::WriteEsdsBox(ov::ByteStream &container_stream)
	{
		// ISO/IEC 14496-14 5.6.1
		// aligned(8) class ESDBox extends FullBox(‘esds’, version = 0, 0)
		// {
		// 	ES_Descriptor ES;
		// }

		ov::ByteStream stream(4096);

		if (WriteESDesciptor(stream) == false) //nl
		{
			return false;
		}

		return WriteFullBox(container_stream, "esds", *stream.GetData(), 0, 0);
	}

	bool Packager::WriteESDesciptor(ov::ByteStream &container_stream)
	{
		// ISO/IEC 14496-1 7.2.2.2.1
		// abstract aligned(8) expandable(2^28 - 1) class BaseDescriptor : bit(8) tag = 0
		// {
		// 	// empty. To be filled by classes extending this class.
		// }

		// ISO/IEC 14496-1 7.2.6.5.1
		// class ES_Descriptor extends BaseDescriptor : bit(8) tag = ES_DescrTag (0x03)
		// {
		// 	bit(16) ES_ID;
		// 	bit(1) streamDependenceFlag;
		// 	bit(1) URL_Flag;
		// 	bit(1) OCRstreamFlag;
		// 	bit(5) streamPriority;
		// 	if (streamDependenceFlag)
		// 		bit(16) dependsOn_ES_ID;
		// 	if (URL_Flag)
		// 	{
		// 		bit(8) URLlength;
		// 		bit(8) URLstring[URLlength];
		// 	}
		// 	if (OCRstreamFlag)
		// 		bit(16) OCR_ES_Id;
		// 	DecoderConfigDescriptor decConfigDescr;
		// 	if (ODProfileLevelIndication == 0x01)  //no SL extension.
		// 	{
		// 		SLConfigDescriptor slConfigDescr;
		// 	}
		// 	else  // SL extension is possible.
		// 	{
		// 		SLConfigDescriptor slConfigDescr;
		// 	}
		// 	IPI_DescrPointer ipiPtr[0..1];
		// 	IP_IdentificationDataSet ipIDS[0..255];
		// 	IPMP_DescriptorPointer ipmpDescrPtr[0..255];
		// 	LanguageDescriptor langDescr[0..255];
		// 	QoS_Descriptor qosDescr[0..1];
		// 	RegistrationDescriptor regDescr[0..1];
		// 	ExtensionDescriptor extDescr[0..255];
		// }

		// ESDescriptor
		// {
		// 	esId = 1, streamDependenceFlag = 0, URLFlag = 0, oCRstreamFlag = 0, streamPriority = 0, URLLength = 0, URLString = 'null', remoteODFlag = 0, dependsOnEsId = 0, oCREsId = 0, 

		// 		decoderConfigDescriptor = DecoderConfigDescriptor
		// 		{
		// 			objectTypeIndication = 64, streamType = 5, upStream = 0, bufferSizeDB = 0, maxBitRate = 0, avgBitRate = 0,
		// 			decoderSpecificInfo = null, 
		//
		// 			audioSpecificInfo = AudioSpecificConfig
		// 			{
		// 				configBytes = 119056E500, audioObjectType = 2(AAC LC), samplingFrequencyIndex = 3(48000), samplingFrequency = 0, channelConfiguration = 2, extensionAudioObjectType = 5(SBR), sbrPresentFlag = false, psPresentFlag = false, extensionSamplingFrequencyIndex = -1(null), extensionSamplingFrequency = 0, extensionChannelConfiguration = 0, syncExtensionType = 695, frameLengthFlag = 0, dependsOnCoreCoder = 0, coreCoderDelay = 0, extensionFlag = 0, layerNr = 0, numOfSubFrame = 0, layer_length = 0, aacSectionDataResilienceFlag = false, aacScalefactorDataResilienceFlag = false, aacSpectralDataResilienceFlag = false, extensionFlag3 = 0
		// 			}, 
		// 			configDescriptorDeadBytes =, profileLevelIndicationDescriptors = []
		// 		}, 

		// 		slConfigDescriptor = SLConfigDescriptor
		// 		{
		// 			predefined = 2
		// 		}
		// }

		ov::ByteStream stream(4096);

		// uint(16) ES_ID;
		stream.WriteBE16(GetTrack()->GetId()+1);
		// bit(1) streamDependenceFlag; disabled
		// bit(1) URL_Flag; disabled
		// bit(1) OCRstreamFlag; disabled
		// bit(5) streamPriority; zero
		stream.Write8(0);

		// DecoderConfigDescriptor decConfigDescr;
		if (WriteDecoderConfigDescriptor(stream) == false)
		{
			return false;
		}

		// SLConfigDescriptor slConfigDescr;
		if (WriteSLConfigDescriptor(stream) == false)
		{
			return false;
		}

		return WriteBaseDescriptor(container_stream, 0x03, *stream.GetData());		
	}
	
	bool Packager::WriteDecoderConfigDescriptor(ov::ByteStream &container_stream)
	{
		// ISO/IEC 14496-1 7.2.6.6
		// class DecoderConfigDescriptor extends BaseDescriptor : bit(8) tag = DecoderConfigDescrTag (0x04)
		// {
		// 	bit(8) objectTypeIndication;
		// 	bit(6) streamType;
		// 	bit(1) upStream;
		// 	const bit(1) reserved = 1;

		// 	bit(24) bufferSizeDB;
		// 	bit(32) maxBitrate;
		// 	bit(32) avgBitrate;
		// 	DecoderSpecificInfo decSpecificInfo[0..1];
		// 	profileLevelIndicationIndexDescriptor profileLevelIndicationIndexDescr [0..255];
		// }

		ov::ByteStream stream(4096);

		// uint(8) objectTypeIndication;
		if (GetTrack()->GetCodecId() == cmn::MediaCodecId::Aac)
		{
			stream.Write8(0x40); // Audio ISO/IEC 14496-3
		}
		else
		{
			// Not supported yet
			return false;
		}

		// bit(6) streamType; 0x04 = VisualStream 0x05 = AudioStream
		// bit(1) upStream; 0 = downstream
		// const bit(1) reserved = 1;
		stream.Write8(0x05 << 2 | 0x01);

		// bit(24) bufferSizeDB;
		stream.WriteBE24(0);

		// bit(32) maxBitrate;
		stream.WriteBE32(GetTrack()->GetBitrate());

		// bit(32) avgBitrate;
		stream.WriteBE32(GetTrack()->GetBitrate());

		// DecoderSpecificInfo decSpecificInfo[0..1];
		if (WriteAudioSpecificInfo(stream) == false)
		{
			return false;
		}

		// profileLevelIndicationIndexDescriptor profileLevelIndicationIndexDescr [0..255];
		// Skip

		return WriteBaseDescriptor(container_stream, 0x04, *stream.GetData());
	}

	bool Packager::WriteAudioSpecificInfo(ov::ByteStream &container_stream)
	{
		// abstract class DecoderSpecificInfo extends BaseDescriptor : bit(8) tag = DecSpecificInfoTag(0x05)
		// {
		// 	// empty. To be filled by classes extending this class.
		// }

		// ISO/IEC 14496-3 1.6.2.1 Table 1.16
		// GetAudioObjectType()
		// {
		// 	audioObjectType; 5 uimsbf 
		// 	if (audioObjectType == 31)
		// 	{
		// 		audioObjectType = 32 + audioObjectTypeExt; 6 uimsbf
		// 	}
		// 	return audioObjectType;
		// }

		// ISO / IEC 14496 - 3 1.6.2.1 Table 1.15 AudioSpecificConfig() extends DecoderSpecificInfo
		// {
		// 	audioObjectType = GetAudioObjectType();
		// 	samplingFrequencyIndex; 4 bslbf 
		// 	if (samplingFrequencyIndex == 0xf)
		// 	{
		// 		samplingFrequency; 24 uimsbf
		// 	}
		// 	channelConfiguration; 4 bslbf
		////////////////////
		// O M I T T E D - If ObjectType is 1(AAC MAIN) or 2(AAC LC), it is used up to this point.
		////////////////////
		
		// uint(5) audioObjectType; [1] AAC MAIN [2] AAC LC [3] AAC SSR [4] AAC LTP [5] SBR [6] AAC Scalable [7] TwinVQ [8] CELP
		// uint(4) samplingFrequencyIndex; [0] 96000 [1] 88200 [2] 64000 [3] 48000 [4] 44100 [5] 32000 [6] 24000 [7] 22050 [8] 16000 [9] 12000 [10] 11025 [11] 8000 [12] 7350 
		// uint(4) channelConfiguration; 

		auto aac_config = GetTrack()->GetAacConfig();
		if (aac_config == nullptr)
		{
			return false;
		}
		
		ov::BitWriter bit_writer(2);
		bit_writer.Write(5, static_cast<uint8_t>(aac_config->ObjectType()));
		bit_writer.Write(4, static_cast<uint8_t>(aac_config->SamplingFrequency()));
		bit_writer.Write(4, static_cast<uint8_t>(aac_config->Channel()));

		return WriteBaseDescriptor(container_stream, 0x05, ov::Data(bit_writer.GetData(), bit_writer.GetDataSize()));
	}

	bool Packager::WriteSLConfigDescriptor(ov::ByteStream &container_stream)
	{
		// ISO/IEC 14496-1 7.3.2.3
		// class SLConfigDescriptor extends BaseDescriptor : bit(8) tag = SLConfigDescrTag (0x06)
		// {
		// 	bit(8) predefined;
		// 	if (predefined == 0)
		// 	{
		// 		bit(1) useAccessUnitStartFlag;
		// 		bit(1) useAccessUnitEndFlag;
		// 		bit(1) useRandomAccessPointFlag;
		// 		bit(1) hasRandomAccessUnitsOnlyFlag;
		// 		bit(1) usePaddingFlag;
		// 		bit(1) useTimeStampsFlag;
		// 		bit(1) useIdleFlag;
		// 		bit(1) durationFlag;
		// 		bit(32) timeStampResolution;
		// 		bit(32) OCRResolution;
		// 		bit(8) timeStampLength;	 // must be ≤ 64
		// 		bit(8) OCRLength;		 // must be ≤ 64
		// 		bit(8) AU_Length;		 // must be ≤ 32
		// 		bit(8) instantBitrateLength;
		// 		bit(4) degradationPriorityLength;
		// 		bit(5) AU_seqNumLength;		// must be ≤ 16
		// 		bit(5) packetSeqNumLength;	// must be ≤ 16
		// 		bit(2) reserved = 0b11;
		// 	}
		// 	if (durationFlag)
		// 	{
		// 		bit(32) timeScale;
		// 		bit(16) accessUnitDuration;
		// 		bit(16) compositionUnitDuration;
		// 	}
		// 	if (!useTimeStampsFlag)
		// 	{
		// 		bit(timeStampLength) startDecodingTimeStamp;
		// 		bit(timeStampLength) startCompositionTimeStamp;
		// 	}
		// }

		// Predefined field value Description
		// 0x00 Custom
		// 0x01 null SL packet header
		// 0x02 Reserved for use in MP4 files (*)
		// 0x03 – 0xFF Reserved for ISO use

		ov::ByteStream stream(4);

		// bit(8) predefined;
		stream.Write8(0x02); // MP4 file

		return WriteBaseDescriptor(container_stream, 0x06, *stream.GetData());
	}

	bool Packager::WriteSttsBox(ov::ByteStream &container_stream)
	{
		// ISO/IEC 14496-12 8.6.1.2 
		// aligned(8) class TimeToSampleBox	extends FullBox(’stts’, version = 0, 0)
		// {
		// 	unsigned int(32) entry_count;
		// 	int i;
		// 	for (i = 0; i < entry_count; i++)
		// 	{
		// 		unsigned int(32) sample_count;
		// 		unsigned int(32) sample_delta;
		// 	}
		// }

		ov::ByteStream stream(4);

		// unsigned int(32) entry_count;
		stream.WriteBE32(0);

		return WriteFullBox(container_stream, "stts", *stream.GetData(), 0, 0);
	}

	bool Packager::WriteStscBox(ov::ByteStream &container_stream)
	{
		// ISO/IEC 14496-12 8.7.4.2
		// aligned(8) class SampleToChunkBox extends FullBox(‘stsc’, version = 0, 0)
		// {
		// 	unsigned int(32) entry_count;
		// 	for (i = 1; i <= entry_count; i++)
		// 	{
		// 		unsigned int(32) first_chunk;
		// 		unsigned int(32) samples_per_chunk;
		// 		unsigned int(32) sample_description_index;
		// 	}
		// }

		ov::ByteStream stream(4);

		// unsigned int(32) entry_count;
		stream.WriteBE32(0);

		return WriteFullBox(container_stream, "stsc", *stream.GetData(), 0, 0);
	}

	bool Packager::WriteStszBox(ov::ByteStream &container_stream)
	{
		// ISO/IEC 14496-12 8.7.3.2
		// aligned(8) class SampleSizeBox extends FullBox(‘stsz’, version = 0, 0)
		// {
		// 	unsigned int(32) sample_size;
		// 	unsigned int(32) sample_count;
		// 	if (sample_size == 0)
		// 	{
		// 		for (i = 1; i <= sample_count; i++)
		// 		{
		// 			unsigned int(32) entry_size;
		// 		}
		// 	}
		// }

		ov::ByteStream stream(16);

		// unsigned int(32) sample_size;
		// If this field is set to 0, then the samples have	different sizes, and those sizes are stored in the sample size table.	/// If this	field is not 0,	it specifies the constant sample size, and no array follows
		// TODO(Getroot) : What should I put in the sample size value?
		stream.WriteBE32(0);

		// unsigned int(32) sample_count;
		// if sample‐size is 0,	then it	is also the	number of entries in the following table.
		stream.WriteBE32(0);

		return WriteFullBox(container_stream, "stsz", *stream.GetData(), 0, 0);
	}

	bool Packager::WriteStcoBox(ov::ByteStream &container_stream)
	{
		// ISO/IEC 14496-12 8.7.5.2
		// aligned(8) class ChunkOffsetBox extends FullBox(‘stco’, version = 0, 0)
		// {
		// 	unsigned int(32) entry_count;
		// 	for (i = 1; i <= entry_count; i++)
		// 	{
		// 		unsigned int(32) chunk_offset;
		// 	}
		// }

		ov::ByteStream stream(8);

		// unsigned int(32) entry_count;
		stream.WriteBE32(0);

		return WriteFullBox(container_stream, "stco", *stream.GetData(), 0, 0);
	}

	bool Packager::WriteMvexBox(ov::ByteStream &container_stream)
	{
		// ISO/IEC 14496-12 8.8.1
		// aligned(8) class MovieExtendsBox extends Box(‘mvex’)
		// {
		// }

		ov::ByteStream stream(4096);
		
		if (WriteTrexBox(stream) == false)
		{
			logtw("Failed to write trex box");
			return false;
		}

		return WriteBox(container_stream, "mvex", *stream.GetData());
	}

	bool Packager::WriteTrexBox(ov::ByteStream &container_stream)
	{
		// ISO/IEC 14496-12 8.8.3
		// aligned(8) class TrackExtendsBox extends FullBox(‘trex’, 0, 0)
		// {
		// 	unsigned int(32) track_ID;
		// 	unsigned int(32) default_sample_description_index;
		// 	unsigned int(32) default_sample_duration;
		// 	unsigned int(32) default_sample_size;
		// 	unsigned int(32) default_sample_flags;
		// }

		ov::ByteStream stream(24);

		// unsigned int(32) track_ID;
		stream.WriteBE32(GetTrack()->GetId()+1);

		// unsigned int(32) default_sample_description_index;
		stream.WriteBE32(1);

		// unsigned int(32) default_sample_duration;
		stream.WriteBE32(0);

		// unsigned int(32) default_sample_size;
		stream.WriteBE32(1);

		// unsigned int(32) default_sample_flags;
		// bit(4) reserved = 0;
		// unsigned int(2) is_leading;
		// unsigned int(2) sample_depends_on;
		// unsigned int(2) sample_is_depended_on;
		// unsigned int(2) sample_has_redundancy;
		// bit(3) sample_padding_value;
		// bit(1) sample_is_non_sync_sample;
		// unsigned int(16) sample_degradation_priority;
		stream.WriteBE32(0);

		return WriteFullBox(container_stream, "trex", *stream.GetData(), 0, 0);
	}

	bool Packager::WriteMoofBox(ov::ByteStream &container_stream, const std::shared_ptr<const Samples> &samples)
	{
		// ISO/IEC 14496-12 8.8.4
		// aligned(8) class MovieFragmentBox extends Box(‘moof’)
		// {
		// }

		if (samples->IsEmpty() == true)
		{
			logtw("Could not write moof box because input samples list is empty");
			return false;
		}

		ov::ByteStream stream(4096);

		if (WriteMfhdBox(stream, samples) == false)
		{
			logtw("Failed to write mfhd box");
			return false;
		}

		if (WriteTrafBox(stream, samples) == false)
		{
			logtw("Failed to write traf box");
			return false;
		}

		if (WriteBox(container_stream, "moof", *stream.GetData()) == false)
		{
			logtw("Failed to write moof box");
			return false;
		}

		// Update the data_offset field of the Trun box
		// off_t offset = (-1 * (off_t)_last_trun_box_size) + (off_t)BMFF_FULL_BOX_HEADER_SIZE + (off_t)4/*[sample_count] field size*/;
		// container_stream.MoveOffset(offset);
		// container_stream.WriteBE32(container_stream.GetLength() + BMFF_BOX_HEADER_SIZE /* mdat header size */);
		// // Restore
		// container_stream.SetOffset(container_stream.GetLength());

		auto container = container_stream.GetDataPointer();
		auto p = container->GetWritableDataAs<uint8_t>();
		auto pos = container_stream.GetLength() - _last_trun_box_size + BMFF_FULL_BOX_HEADER_SIZE + 4;

		ByteWriter<uint32_t>::WriteBigEndian(p + pos, container_stream.GetLength() + BMFF_BOX_HEADER_SIZE /* mdat header size */);

		return true;
	}

	bool Packager::WriteMfhdBox(ov::ByteStream &container_stream, const std::shared_ptr<const Samples> &samples)
	{
		// ISO/IEC 14496-12 8.8.5
		// aligned(8) class MovieFragmentHeaderBox extends FullBox(‘mfhd’, 0, 0)
		// {
		// 	unsigned int(32) sequence_number;
		// }

		ov::ByteStream stream(8);

		// unsigned int(32) sequence_number;
		stream.WriteBE32(_sequence_number++);

		return WriteFullBox(container_stream, "mfhd", *stream.GetData(), 0, 0);
	}

	bool Packager::WriteTrafBox(ov::ByteStream &container_stream, const std::shared_ptr<const Samples> &samples)
	{
		// ISO/IEC 14496-12 8.8.6
		// aligned(8) class TrackFragmentBox extends Box(‘traf’)
		// {
		// }

		ov::ByteStream stream(4096);

		if (WriteTfhdBox(stream, samples) == false)
		{
			logtw("Failed to write tfhd box");
			return false;
		}

		if (WriteTfdtBox(stream, samples) == false)
		{
			logtw("Failed to write tfdt box");
			return false;
		}

		if (WriteTrunBox(stream, samples) == false)
		{
			logtw("Failed to write trun box");
			return false;
		}

		return WriteBox(container_stream, "traf", *stream.GetData());
	}

	bool Packager::WriteTfhdBox(ov::ByteStream &container_stream, const std::shared_ptr<const Samples> &samples)
	{
		// ISO/IEC 14496-12 8.8.7
		// aligned(8) class TrackFragmentHeaderBox extends FullBox(‘tfhd’, 0, tf_flags)
		// {
		// 	unsigned int(32) track_ID;
		// 	// all the following are optional fields
		// 	unsigned int(64) base_data_offset;
		// 	unsigned int(32) sample_description_index;
		// 	unsigned int(32) default_sample_duration;
		// 	unsigned int(32) default_sample_size;
		// 	unsigned int(32) default_sample_flags
		// }

		ov::ByteStream stream(64);

		// unsigned int(32) track_ID;
		stream.WriteBE32(GetTrack()->GetId()+1);

		// unsigned int(64) base_data_offset;

		// unsigned int(32) sample_description_index;
		stream.WriteBE32(1);

		// unsigned int(32) default_sample_duration;
		stream.WriteBE32(33);

		// unsigned int(32) default_sample_size;
		stream.WriteBE32(0);

		// unsigned int(32) default_sample_flags;
		stream.WriteBE32(0);

		// tf_flags
		// 0x000001 base-data-offset-present:
		// 0x000002 sample-description-index-present:
		// 0x000008 default-sample-duration-present
		// 0x000010 default-sample-size-present
		// 0x000020 default-sample-flags-present
		// 0x010000 duration-is-empty:
		// 0x020000 default‐base‐is‐moof: if base‐data‐offset‐present is 1, this flag is ignored. If base-data-offset-present is zero, this indicates that the base-data-offset for this track fragment is the position of the first byte of the enclosing Movie Fragment Box. Support for the default‐base‐is‐moof flag is required under the ‘iso5’ brand, and it shall not be used in brands or compatible brands earlier than iso5.
		
		return WriteFullBox(container_stream, "tfhd", *stream.GetData(), 0, 0x2 | 0x8 | 0x10 | 0x20 |0x020000);
	}

	bool Packager::WriteTfdtBox(ov::ByteStream &container_stream, const std::shared_ptr<const Samples> &samples)
	{
		// ISO/IEC 14496-12 8.8.12
		// aligned(8) class TrackFragmentBaseMediaDecodeTimeBox	extends FullBox(‘tfdt’, version, 0)
		// {
		// 	if (version == 1)
		// 	{
		// 		unsigned int(64) baseMediaDecodeTime;
		// 	}
		// 	else
		// 	{  // version==0
		// 		unsigned int(32) baseMediaDecodeTime;
		// 	}
		// }

		ov::ByteStream stream(32);

		// unsigned int(64) baseMediaDecodeTime;
		// baseMediaDecodeTime is an integer equal to the sum of the decode durations of all earlier samples in the media, 
		// expressed in the media's timescale. It does not include the samples added in the enclosing track fragment.
		if (samples->IsEmpty() == true)
		{
			return false;
		}

		auto base_media_decode_time = samples->GetAt(0)->GetDts();
		stream.WriteBE64(base_media_decode_time);

		return WriteFullBox(container_stream, "tfdt", *stream.GetData(), 1, 0);
	}

	bool Packager::WriteTrunBox(ov::ByteStream &container_stream, const std::shared_ptr<const Samples> &samples)
	{
		// ISO/IEC 14496-12 8.8.8
		// aligned(8) class TrackRunBox	extends FullBox(‘trun’, version, tr_flags)
		// {
		// 	unsigned int(32) sample_count;
		//
		// 	// the following are optional fields
		// 	signed int(32) data_offset;
		// 	unsigned int(32) first_sample_flags;
		// 	// all fields in the following array are optional
		// 	{
		// 		unsigned int(32) sample_duration;
		// 		unsigned int(32) sample_size;
		// 		unsigned int(32) sample_flags 
		//		if (version == 0)
		// 		{
		// 			unsigned int(32) sample_composition_time_offset;
		// 		}
		// 		else
		// 		{
		// 			signed int(32) sample_composition_time_offset;
		// 		}
		// 	} [sample_count]
		// }

		// [tr_flags]
		// 0x000001 data-offset-present.
		// 0x000004 first-sample-flags-present; this over-rides the default flags for the first sample only. This makes it possible to record a group of frames where the first is a key and the rest are difference frames, without supplying explicit flags for every sample. If this flag and field are used, sample‐flags shall not be present.
		// 0x000100 sample‐duration‐present: indicates that each sample has its own duration, otherwise the default is used.
		// 0x000200 sample‐size‐present: each sample has its own size, otherwise the default is used.
		// 0x000400 sample-flags-present; each sample has its own flags, otherwise the default is used.
		// 0x000800 sample-composition-time-offsets-present; each sample has a composition time offset (e.g. as used for I/P/B video in MPEG).

		uint32_t tr_flags;
		if (GetTrack()->GetMediaType() == cmn::MediaType::Video)
		{
			tr_flags = 0x000001 | 0x000100 | 0x000200 | 0x000400 | 0x000800;
		}
		else
		{
			tr_flags = 0x000001 | 0x000100 | 0x000200;
		}

		// [Semantics]
		// sample_count the number of samples being added in this run; also the number of rows in the following table (the rows can be empty)
		// data_offset is added to the implicit or explicit data_offset established in the track fragment header.
		//		- This is the distance from the start of moof to data.
		// first_sample_flags provides a set of flags for the first sample only of this run.

		ov::ByteStream stream(1024);

		// unsigned int(32) sample_count;
		stream.WriteBE32(samples->GetTotalCount());

		// signed int(32) data_offset;
		// Note(Getroot): This is not required for BMFF, but required for MS Smooth Streaming. (https://docs.microsoft.com/en-us/openspecs/windows_protocols/ms-sstr/6d796f37-b4f0-475f-becd-13f1c86c2d1f) 
		// Therefore, it is assumed that some players may not be able to play normally without an offset.

		// sizeof(Moof box) + Mdat box header(8)
		// It will be updated after writing the whole Moof box.
		stream.WriteBE32(0); 
		
		for (const auto &sample : samples->GetList())
		{
			// unsigned int(32) sample_duration;
			stream.WriteBE32(sample->GetDuration());

			if (GetTrack()->GetMediaType() == cmn::MediaType::Video)
			{
				// unsigned int(32) sample_size;
				stream.WriteBE32(sample->GetData()->GetLength());

				// unsigned int(32) sample_flags;
				uint32_t sample_flags = 0;
				GetSampleFlags(sample, sample_flags);
				stream.WriteBE32(sample_flags);

				// unsigned int(32) sample_composition_time_offset;
				stream.WriteBE32(int32_t(sample->GetPts() - sample->GetDts()));
			}
			else
			{
				stream.WriteBE32(sample->GetData()->GetLength());
			}
		}

		// Store the size of the last trun box in _last_trun_box_size below. Since the trun box is positioned at the end of the moof, 
		// it is used to find the position of the data_offset of the trun box in reverse.
		_last_trun_box_size = BMFF_FULL_BOX_HEADER_SIZE + stream.GetOffset();

		return WriteFullBox(container_stream, "trun", *stream.GetData(), 1, tr_flags);
	}

	bool Packager::GetSampleFlags(const std::shared_ptr<const MediaPacket> &sample, uint32_t &flags)
	{
		// ISO/IEC 14496-12 8.8.3
		// 
		// The sample flags field in sample fragments (default_sample_flags here and in a Track Fragment Header Box, 
		// and sample_flags and first_sample_flags in a Track Fragment Run Box) is coded as a 32-bit value. It has the 
		// following structure:
		//
		// bit(4) reserved = 0;
		// unsigned int(2) is_leading;
		// unsigned int(2) sample_depends_on;
		// unsigned int(2) sample_is_depended_on;
		// unsigned int(2) sample_has_redundancy;
		// bit(3) sample_padding_value;
		// bit(1) sample_is_non_sync_sample;
		// unsigned int(16) sample_degradation_priority;

		// is_leading takes one of the following four values : 
		// 0 : the leading nature of this sample is unknown; therefore not decodable);
		// 1 : this sample is a leading sample that has a dependency before the referenced I - picture(and is therefore not decodable);
		// 2 : this sample is not a leading sample;
		// 3 : this sample is a leading sample that has no dependency before the referenced I - picture(and is therefore decodable);

		// sample_depends_on takes one of the following four values : 
		// 0 : the dependency of this sample is unknown;
		// 1 : this sample does depend on others(not an I picture);
		// 2 : this sample does not depend on others(I picture);
		// 3 : reserved

		// sample_is_depended_on takes one of the following four values ​​ : 
		// 0 : the dependency of other samples on this sample is unknown;
		// 1 : other samples may depend on this one(not disposable);
		// 2 : no other sample depends on this one(disposable);
		// 3 : reserved

		// sample_has_redundancy takes one of the following four values ​​ : 
		// 0 : it is unknown whether there is redundant coding in this sample;
		// 1 : there is redundant coding in this sample;
		// 2 : there is no redundant coding in this sample;
		// 3 : reserved

		// The flag [sample_is_non_sync_sample] provides the same information as the sync sample table [8.6.2]. When this value is set 0 for a sample, it is the same as if the sample were not in a movie fragment and marked with an entry in the sync sample table (or, if all samples are sync samples, the sync sample table were absent).

		// The [sample_padding_value] is defined as for the padding bits table. The sample_degradation_priority is defined as for the degradation priority table.

		flags = 0;

		if (sample->GetMediaType() == cmn::MediaType::Video && sample->GetFlag() == MediaPacketFlag::Key)
		{
			flags = 0x02000000; // sample_depends_on = 2
		}
		else
		{
			flags = 0x01010000; // sample_depends_on = 1, sample_is_non_sync_sample = 1
		}

		return true;
	}

	bool Packager::WriteMdatBox(ov::ByteStream &container_stream, const std::shared_ptr<const Samples> &samples)
	{
		// ISO/IEC 14496-12 8.1.1
		// aligned(8) class MediaDataBox extends Box(‘mdat’)
		// {
		// 	bit(8) data[];
		// }
		auto data = std::make_shared<ov::Data>(samples->GetTotalSize());
		for (const auto &sample : samples->GetList())
		{
			data->Append(sample->GetData());
		}

		return WriteBox(container_stream, "mdat", *data);
	}
	
	bool Packager::WriteBaseDescriptor(ov::ByteStream &stream, uint8_t tag, const ov::Data &data)
	{
		// ISO/IEC 14496-1 7.2.2.2
		// abstract aligned(8) expandable(228 - 1) class BaseDescriptor : bit(8) tag = 0
		// {
		// 	// empty. To be filled by classes extending this class.
		// }

		// uint(8) tag;
		stream.Write8(tag);

		// length;
		// ISO 14491-1 8.3.3 Expandable classes
		// The size encoding precedes any parsable variables of the class. If the class has an object_id, the encoding
		// of the object_id precedes the size encoding. The size information shall not include the number of bytes
		// needed for the size and the object_id encoding. Instances of expandable classes shall always have a size
		// corresponding to an integer number of bytes. The size information is accessible within the class as class
		// instance variable sizeOfInstance. 

		size_t size_of_instance = data.GetLength();
		
		// Find the number of bits to express a number with the log function, 
		// and divide by 7 to calculate how many bytes it is split into after encoding
		int offset = (::log2(size_of_instance) / 7);
		auto remainder = size_of_instance;

		while (offset > 0)
		{
			// MSB is set to 0 only for the last byte, and MSB is set to 1 for the rest of the byte.
			auto value = 0x80 | ((remainder >> (offset-- * 7)) & 0x7f);
			stream.Write8(value);
		}

		auto value = remainder & 0x7f;
		stream.Write8(value);

		// bit(8) data[length];
		stream.Write(data.GetData(), data.GetLength());

		return true;
	}
	
	// Write Box
	bool Packager::WriteBox(ov::ByteStream &stream, const ov::String &box_name, const ov::Data &box_data)
	{
		// ISO/IEC 14496-12 4.2
		// aligned(8) class Box(unsigned int(32) boxtype, optional unsigned int(8)[16] extended_type)
		// {
		// 	unsigned int(32) size;
		// 	unsigned int(32) type = boxtype;
		// 	if (size == 1)
		// 	{
		// 		unsigned int(64) largesize;
		// 	}
		// 	else if (size == 0)
		// 	{
		// 		// box extends to end of file
		// 	}
		// 	if (boxtype ==‘uuid’)
		// 	{
		// 		unsigned int(8)[16] usertype = extended_type;
		// 	}
		// }

		// box_name must be 4 bytes or less
		if (box_name.GetLength() != 4)
		{
			// Assert
			OV_ASSERT2(false);
			return false;
		}

		auto box_size = box_data.GetLength() + BMFF_BOX_HEADER_SIZE;
		stream.WriteBE32(box_size);
		stream.WriteText(box_name);

		// box_data
		return stream.Write(box_data.GetData(), box_data.GetLength());
	}

	// Write Full Box
	bool Packager::WriteFullBox(ov::ByteStream &stream, const ov::String &box_name, const ov::Data &box_data, uint8_t version, uint32_t flags)
	{
		// ISO/IEC 14496-12 4.2
		// aligned(8) class FullBox(unsigned int(32) boxtype, unsigned int(8) v, bit(24) f) extends	Box(boxtype)
		// {
		// 	unsigned int(8) version = v;
		// 	bit(24) flags = f;
		// }

		// box_name must be 4 bytes or less
		if (box_name.GetLength() > 4)
		{
			// Assert
			OV_ASSERT2(false);
			return false;
		}

		auto box_size = box_data.GetLength() + BMFF_FULL_BOX_HEADER_SIZE;

		stream.WriteBE32(box_size);
		stream.WriteText(box_name);
		stream.Write8(version);
		stream.WriteBE24(flags);

		// box_data
		return stream.Write(box_data.GetData(), box_data.GetLength());
	}

} // namespace bmff
	