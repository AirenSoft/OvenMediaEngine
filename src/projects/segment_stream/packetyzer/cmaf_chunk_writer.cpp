//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2019 AirenSoft. All rights reserved.
//
//==============================================================================

#include "cmaf_chunk_writer.h"

#define DEFAULT_CHUNK_HEADER_SIZE (256)

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
CmafChunkWriter::CmafChunkWriter(M4sMediaType media_type,
											 uint32_t sequence_number,
											 uint32_t track_id,
											 bool http_chunked_transfer_support)
												: M4sWriter(media_type)
{
  	_sequence_number = sequence_number;
	_track_id = track_id;
	_http_chunked_transfer_support = http_chunked_transfer_support;
}

//====================================================================================================
// Destructor
//====================================================================================================
CmafChunkWriter::~CmafChunkWriter( )
{
}
//====================================================================================================
//  GetHttpChunkedSegmentBody
// - make http chunked transfer body
//====================================================================================================
std::shared_ptr<ov::Data> CmafChunkWriter::GetChunkedSegment()
{
	if(_total_chunk_data_size == 0)
		return nullptr;

	auto chunked_segment = std::make_shared<ov::Data>(_total_chunk_data_size + 5);

	for(const auto &data : _chunk_data_list)
	{
		chunked_segment->Append(data->GetData(), data->GetLength());
	}

	if(_http_chunked_transfer_support)
	{
		chunked_segment->Append("0\r\n\r\n", 5);
	}

	return chunked_segment;
}

//====================================================================================================
//  CreateData
//====================================================================================================
int CmafChunkWriter::AppendSamples(const std::vector<std::shared_ptr<SampleData>> &sample_datas)
{
	int write_size = 0;

	for(auto &sample_data : sample_datas)
    {
        auto chunk_data = AppendSample(sample_data);

        if(chunk_data != nullptr)
        	write_size += chunk_data->GetLength();
    }

	return write_size;
}

//====================================================================================================
//  AppendSample
//====================================================================================================
const std::shared_ptr<ov::Data> CmafChunkWriter::AppendSample(const std::shared_ptr<SampleData> &sample_data)
{
    if(!_write_started)
    {
        _write_started = true;
        _start_timestamp = sample_data->timestamp;
    }

	auto chunk_stream = std::make_shared<ov::Data>(sample_data->data->GetLength() + DEFAULT_CHUNK_HEADER_SIZE);

    // moof box write
    MoofBoxWrite(chunk_stream, sample_data);

    // mdata box write
    MdatBoxWrite(chunk_stream, sample_data->data);

    if(_http_chunked_transfer_support)
	{
		chunk_stream->Insert(ov::String::FormatString("%x\r\n", chunk_stream->GetLength()).ToData(false).get(), 0);
		chunk_stream->Append(reinterpret_cast<const uint8_t *>("\r\n"), 2);
	}

	_chunk_data_list.push_back(chunk_stream);

    // total data length
	_total_chunk_data_size += chunk_stream->GetLength();

	return chunk_stream;
}

//====================================================================================================
// moof(Movie Fragment)
//====================================================================================================
int CmafChunkWriter::MoofBoxWrite(std::shared_ptr<ov::Data> &data_stream,
									const std::shared_ptr<SampleData> &sample_data)
{
	auto data = std::make_shared<ov::Data>(DEFAULT_CHUNK_HEADER_SIZE);

	MfhdBoxWrite(data);
	TrafBoxWrite(data, sample_data);

	BoxDataWrite("moof", data, data_stream);

	// trun data offset value change
	uint32_t data_offset = data_stream->GetLength() + 8;
	int position = (_media_type == M4sMediaType::Video) ?
				   data_stream->GetLength() - 16 - 4 : data_stream->GetLength() - 8 - 4;

	if(position < 0)
	{
		return -1;
	}

	ByteWriter<uint32_t>::WriteBigEndian(data_stream->GetWritableDataAs<uint8_t>() + position, data_stream->GetLength() + 8);


	return data_stream->GetLength();
}

//====================================================================================================
// mfhd(Movie Fragment Header)
//====================================================================================================
int CmafChunkWriter::MfhdBoxWrite(std::shared_ptr<ov::Data> &data_stream)
{
	auto data = std::make_shared<ov::Data>();

	WriteUint32(_sequence_number++, data);

	return BoxDataWrite("mfhd", 0, 0, data, data_stream);
}

//====================================================================================================
// traf(Track Fragment)
//====================================================================================================
int CmafChunkWriter::TrafBoxWrite(std::shared_ptr<ov::Data> &data_stream,
									const std::shared_ptr<SampleData> &sample_data)
{
	auto data = std::make_shared<ov::Data>();

	TfhdBoxWrite(data);
	TfdtBoxWrite(data, sample_data->timestamp);
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

int CmafChunkWriter::TfhdBoxWrite(std::shared_ptr<ov::Data> &data_stream)
{
	auto data = std::make_shared<ov::Data>();
	uint32_t flag = TFHD_FLAG_DEFAULT_BASE_IS_MOOF;

	WriteUint32(_track_id, data);	// track id

	return BoxDataWrite("tfhd", 0, flag, data, data_stream);
}

//====================================================================================================
// tfdt(Track Fragment Base Media Decode Time)
//====================================================================================================
int CmafChunkWriter::TfdtBoxWrite(std::shared_ptr<ov::Data> &data_stream, uint64_t timestamp)
{
	auto data = std::make_shared<ov::Data>();

	WriteUint64(timestamp, data);    // Base media decode time

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

int CmafChunkWriter::TrunBoxWrite(std::shared_ptr<ov::Data> &data_stream,
									const std::shared_ptr<SampleData> &sample_data)
{
	auto data = std::make_shared<ov::Data>();
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
int CmafChunkWriter::MdatBoxWrite(std::shared_ptr<ov::Data> &data_stream,
										const std::shared_ptr<ov::Data> &frame_data)
{
	return BoxDataWrite("mdat", frame_data, data_stream, _media_type == M4sMediaType::Video ? true : false);

}