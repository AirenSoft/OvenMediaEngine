//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================

#include "cmaf_m4s_fragment_writer.h"

//Fragmented MP4
// data m4s
//    - moof(Movie Fragment)
//        - mfnd(Movie Fragment Header)
//        - traf(Track Fragment)
//            - tfhd(Track Fragment Header)
//            - trun(Track Fragment Run)
//            - sdtp(Independent Samples)
//    - mdata(Media Data)
//    ...
//

//====================================================================================================
// Constructor
//====================================================================================================
CmafM4sFragmentWriter::CmafM4sFragmentWriter(M4sMediaType media_type, uint32_t sequence_number, uint32_t track_id) :
										M4sWriter(media_type)
{
    _data_stream = std::make_shared<std::vector<uint8_t>>();
    _data_stream->reserve(4096);

	_sequence_number    = sequence_number;
	_track_id			= track_id;
}

//====================================================================================================
// Destructor
//====================================================================================================
CmafM4sFragmentWriter::~CmafM4sFragmentWriter( )
{
}

//====================================================================================================
//  CreateData
//====================================================================================================
int CmafM4sFragmentWriter::AppendSamples(const std::vector<std::shared_ptr<FragmentSampleData>> &sample_datas)
{
	int write_size = 0;

	for(auto &sample_data : sample_datas)
    {
        auto chunk_stream = AppendSample(sample_data);

        if(chunk_stream != nullptr)
            write_size += chunk_stream->size();
    }

	return write_size;
}

//====================================================================================================
//  AppendSample
//====================================================================================================
std::shared_ptr<std::vector<uint8_t>> CmafM4sFragmentWriter::AppendSample(const std::shared_ptr<FragmentSampleData> &sample_data)
{
    if(!_write_started)
    {
        _write_started = true;
        _start_timestamp = sample_data->timestamp;
    }

    std::shared_ptr<std::vector<uint8_t>> chunk_stream = std::make_shared<std::vector<uint8_t>>();

    // moof box write
    MoofBoxWrite(chunk_stream, sample_data);

    // trun data offset value change
    uint32_t data_offset = chunk_stream->size() + 8;

    int position = (_media_type == M4sMediaType::Video) ? chunk_stream->size() - 16 - 4 : chunk_stream->size() - 8 - 4;

    if(position < 0)
    {
        return nullptr;
    }

    ByteWriter<uint32_t>::WriteBigEndian(chunk_stream->data() + position, data_offset);

    // mdata box write
    MdatBoxWrite(chunk_stream, sample_data);

    _data_stream->insert(_data_stream->end(), chunk_stream->begin(), chunk_stream->end());

    return chunk_stream;
}

//====================================================================================================
// moof(Movie Fragment)
//====================================================================================================
int CmafM4sFragmentWriter::MoofBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream,
                                        const std::shared_ptr<FragmentSampleData> &sample_data)
{
	std::shared_ptr<std::vector<uint8_t>> data = std::make_shared<std::vector<uint8_t>>();

	MfhdBoxWrite(data);
	TrafBoxWrite(data, sample_data);

	return BoxDataWrite("moof", data, data_stream);
}

//====================================================================================================
// mfhd(Movie Fragment Header)
//====================================================================================================
int CmafM4sFragmentWriter::MfhdBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream)
{
	std::shared_ptr<std::vector<uint8_t>> data = std::make_shared<std::vector<uint8_t>>();

	WriteUint32(_sequence_number++, data);	// Sequence Number

	return BoxDataWrite("mfhd", 0, 0, data, data_stream);
}

//====================================================================================================
// traf(Track Fragment)
//====================================================================================================
int CmafM4sFragmentWriter::TrafBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream,
                                        const std::shared_ptr<FragmentSampleData> &sample_data)
{
	std::shared_ptr<std::vector<uint8_t>> data = std::make_shared<std::vector<uint8_t>>();

	TfhdBoxWrite(data);
	TfdtBoxWrite(data, sample_data);
	TrunBoxWrite(data, sample_data);

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

int CmafM4sFragmentWriter::TfhdBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream)
{
	std::shared_ptr<std::vector<uint8_t>> data = std::make_shared<std::vector<uint8_t>>();
	uint32_t flag = TFHD_FLAG_DEFAULT_BASE_IS_MOOF;

	WriteUint32(_track_id, data);	// track id

	return BoxDataWrite("tfhd", 0, flag, data, data_stream);
}

//====================================================================================================
// tfdt(Track Fragment Base Media Decode Time)
//====================================================================================================
int CmafM4sFragmentWriter::TfdtBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream,
                                        const std::shared_ptr<FragmentSampleData> &sample_data)
{
	std::shared_ptr<std::vector<uint8_t>> data = std::make_shared<std::vector<uint8_t>>();

	WriteUint64(sample_data->timestamp, data);    // Base media decode time

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

int CmafM4sFragmentWriter::TrunBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream,
                                        const std::shared_ptr<FragmentSampleData> &sample_data)
{
	std::shared_ptr<std::vector<uint8_t>> data = std::make_shared<std::vector<uint8_t>>();
	uint32_t flag = 0;

	if (M4sMediaType::Video == _media_type)
	{
		flag = TRUN_FLAG_DATA_OFFSET_PRESENT | TRUN_FLAG_SAMPLE_DURATION_PRESENT | TRUN_FLAG_SAMPLE_SIZE_PRESENT |
		        TRUN_FLAG_SAMPLE_FLAGS_PRESENT | TRUN_FLAG_SAMPLE_COMPOSITION_TIME_OFFSET_PRESENT;
	}
	else if(M4sMediaType::Audio == _media_type)
	{
		flag = TRUN_FLAG_DATA_OFFSET_PRESENT | TRUN_FLAG_SAMPLE_DURATION_PRESENT | TRUN_FLAG_SAMPLE_SIZE_PRESENT;
	}

	WriteUint32(1, data);	                // Sample Item Count;
	WriteUint32(0x11111111, data);	         // Data offset - temp 0 setting

    WriteUint32(sample_data->duration, data); // duration

    if (_media_type == M4sMediaType::Video)
    {
        WriteUint32(sample_data->data->GetLength() + 4, data);	// size + sample
        WriteUint32(sample_data->flag, data);;					// flag
        WriteUint32(sample_data->composition_time_offset, data);	// cts
    }
    else if (_media_type == M4sMediaType::Audio)
    {
        WriteUint32(sample_data->data->GetLength(), data);				// sample
    }

	return BoxDataWrite("trun", 0, flag, data, data_stream);
}

//====================================================================================================
// mdat(Media Data)
//====================================================================================================
int CmafM4sFragmentWriter::MdatBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream,
        const std::shared_ptr<FragmentSampleData> &sample_data)
{
	std::shared_ptr<std::vector<uint8_t>> data = std::make_shared<std::vector<uint8_t>>();

    if (_media_type == M4sMediaType::Video)
    {
        WriteUint32(sample_data->data->GetLength(), data);	// size
    }

    WriteData(sample_data->data, data);

	return BoxDataWrite("mdat", data, data_stream);
}