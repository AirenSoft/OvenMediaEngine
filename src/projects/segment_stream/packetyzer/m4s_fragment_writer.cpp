//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#include "m4s_fragment_writer.h"

//Fragmented MP4
// data m4s
//    - moof(Movie Fragment)
//        - mfnd(Movie Fragment Header)
//        - traf(Track Fragment)
//            - tfhd(Track Fragment Header)
//            - trun(Track Fragment Run)
//            - sdtp(Independent Samples)
//    - mdata(Media Data)

//====================================================================================================
// Constructor
//====================================================================================================
M4sFragmentWriter::M4sFragmentWriter(	M4sMediaType	media_type,
										uint32_t		data_init_size,
										uint32_t		sequence_number,
										uint32_t		track_id,
										uint64_t		start_timestamp,
										std::vector<std::shared_ptr<FragmentSampleData>> &sample_datas) :
										M4sWriter(media_type, data_init_size)
{
	_sequence_number    = sequence_number;
	_track_id			= track_id;
	_start_timestamp	= start_timestamp; 
	_sample_datas	    = sample_datas;
}

//====================================================================================================
// Destructor
//====================================================================================================
M4sFragmentWriter::~M4sFragmentWriter( )
{
	_sample_datas.clear();
}

//====================================================================================================
//  CreateData
//====================================================================================================
int M4sFragmentWriter::CreateData()
{
	int moof_size = 0;
	int mdata_size = 0;

	moof_size += MoofBoxWrite(_data_stream);

	//trun data offset 값 변경
	uint32_t data_offset = _data_stream->size() + 8;
	uint32_t position = 0;

	if (_media_type == M4sMediaType::VideoMediaType)
	    position = _data_stream->size() - _sample_datas.size() * 16 - 4;
	else if (_media_type == M4sMediaType::AudioMediaType)
	    position = _data_stream->size() - _sample_datas.size() * 8 - 4;


	(*_data_stream)[position] = ((uint8_t)(data_offset >> 24 & 0xFF));
	(*_data_stream)[position + 1] = ((uint8_t)(data_offset >> 16 & 0xFF));
	(*_data_stream)[position + 2] = ((uint8_t)(data_offset >> 8 & 0xFF));
	(*_data_stream)[position + 3] = ((uint8_t)(data_offset & 0xFF));

	mdata_size += MdatBoxWrite(_data_stream);

	return moof_size + mdata_size;
}

//====================================================================================================
// moof(Movie Fragment) 
//====================================================================================================
int M4sFragmentWriter::MoofBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream)
{
	std::shared_ptr<std::vector<uint8_t>> data = std::make_shared<std::vector<uint8_t>>();

	MfhdBoxWrite(data);
	TrafBoxWrite(data);
	
	return BoxDataWrite("moof", data, data_stream);
}

//====================================================================================================
// mfhd(Movie Fragment Header) 
//====================================================================================================
int M4sFragmentWriter::MfhdBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream)
{
	std::shared_ptr<std::vector<uint8_t>> data = std::make_shared<std::vector<uint8_t>>();

	WriteUint32(_sequence_number, data);	// Sequence Number

	return BoxDataWrite("mfhd", 0, 0, data, data_stream);
}

//====================================================================================================
// traf(Track Fragment) 
//====================================================================================================
int M4sFragmentWriter::TrafBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream)
{
	std::shared_ptr<std::vector<uint8_t>> data = std::make_shared<std::vector<uint8_t>>();

	TfhdBoxWrite(data);
	TfdtBoxWrite(data);
	TrunBoxWrite(data);

	return BoxDataWrite("traf", data, data_stream);
}

//====================================================================================================
// tfhd(Track Fragment Header) 
//====================================================================================================
#define TFHD_FLAG_BASE_DATA_OFFSET_PRESENT          (0x00001)
#define TFHD_FLAG_SAMPLE_DESCRIPTION_INDEX_PRESENT  (0x00002)
#define TFHD_FLAG_DEFAULT_SAMPLE_DURATION_PRESENT   (0x00008)
#define TFHD_FLAG_DEFAULT_SAMPLE_SIZE_PRESENT       (0x00010)
#define TFHD_FLAG_DEFAULT_SAMPLE_FLAGS_PRESENT      (0x00020)
#define TFHD_FLAG_DURATION_IS_EMPTY                 (0x10000)
#define TFHD_FLAG_DEFAULT_BASE_IS_MOOF              (0x20000)
int M4sFragmentWriter::TfhdBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream)
{
	std::shared_ptr<std::vector<uint8_t>> data = std::make_shared<std::vector<uint8_t>>();
	uint32_t flag = TFHD_FLAG_DEFAULT_BASE_IS_MOOF;

	WriteUint32(_track_id, data);	// track id

	return BoxDataWrite("tfhd", 0, flag, data, data_stream);
}

//====================================================================================================
// tfdt(Track Fragment Base Media Decode Time) 
//====================================================================================================
int M4sFragmentWriter::TfdtBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream)
{
	std::shared_ptr<std::vector<uint8_t>> data = std::make_shared<std::vector<uint8_t>>();
	
	WriteUint64(_start_timestamp, data);    // Base media decode time

	return BoxDataWrite("tfdt", 1, 0, data, data_stream);
}

//====================================================================================================
// Trun(Track Fragment Run) 
//====================================================================================================
#define TRUN_FLAG_DATA_OFFSET_PRESENT                    (0x0001)
#define TRUN_FLAG_FIRST_SAMPLE_FLAGS_PRESENT             (0x0004)
#define TRUN_FLAG_SAMPLE_DURATION_PRESENT                (0x0100)
#define TRUN_FLAG_SAMPLE_SIZE_PRESENT                    (0x0200)
#define TRUN_FLAG_SAMPLE_FLAGS_PRESENT                   (0x0400)
#define TRUN_FLAG_SAMPLE_COMPOSITION_TIME_OFFSET_PRESENT (0x0800)
int M4sFragmentWriter::TrunBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream)
{
	std::shared_ptr<std::vector<uint8_t>> data = std::make_shared<std::vector<uint8_t>>();
	uint32_t flag = 0;

	if (M4sMediaType::VideoMediaType == _media_type)
	{
		flag = TRUN_FLAG_DATA_OFFSET_PRESENT | TRUN_FLAG_SAMPLE_DURATION_PRESENT | TRUN_FLAG_SAMPLE_SIZE_PRESENT | TRUN_FLAG_SAMPLE_FLAGS_PRESENT | TRUN_FLAG_SAMPLE_COMPOSITION_TIME_OFFSET_PRESENT;
	}
	else if(M4sMediaType::AudioMediaType == _media_type)
	{
		flag = TRUN_FLAG_DATA_OFFSET_PRESENT | TRUN_FLAG_SAMPLE_DURATION_PRESENT | TRUN_FLAG_SAMPLE_SIZE_PRESENT;
	}

	WriteUint32(_sample_datas.size(), data);	// Sample Item Count;
	WriteUint32(0x11111111, data);	            // Data offset - temp 0 setting

	for (auto &sample_data : _sample_datas)
	{
		WriteUint32(sample_data->duration, data);					// duration

		if (_media_type == M4sMediaType::VideoMediaType)
		{
			WriteUint32(sample_data->data->GetLength() + 4, data);			// size + sample
			WriteUint32(sample_data->flag, data);;						// flag
			WriteUint32(sample_data->composition_time_offset, data);	// compoistion timeoffset 
		}
		else if (_media_type == M4sMediaType::AudioMediaType)
		{
			WriteUint32(sample_data->data->GetLength(), data);				// sample
		}
	}

	return BoxDataWrite("trun", 0, flag, data, data_stream);
}

//====================================================================================================
// mdat(Media Data) 
//====================================================================================================
int M4sFragmentWriter::MdatBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream)
{
	std::shared_ptr<std::vector<uint8_t>> data = std::make_shared<std::vector<uint8_t>>();

	for (auto &sample_data : _sample_datas)
	{
		if (_media_type == M4sMediaType::VideoMediaType)
		{
			WriteUint32(sample_data->data->GetLength(), data);	// size
		}

		WriteData(sample_data->data, data);
	}
	return BoxDataWrite("mdat", data, data_stream);
}