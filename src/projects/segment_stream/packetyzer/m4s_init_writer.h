//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once

#include "m4s_writer.h"

//====================================================================================================
// M4sWriter
//====================================================================================================
class M4sInitWriter : public M4sWriter
{
public:
	M4sInitWriter(	M4sMediaType 	media_type,
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
					uint16_t        audio_sample_rate) ;


	~M4sInitWriter() override = default;

public :
	int CreateData();

protected :
	int FtypBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream);
	int MoovBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream);

	int MvhdBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream);

	int TrakBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream);
	int TkhdBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream);
	//int EdtsBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream);
	//int ElstBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream);
	int MdiaBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream);
	int MdhdBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream);
	int HdlrBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream);
	int MinfBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream);

	int VmhdBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream);
	int SmhdBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream);

	int DinfBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream);
	int DrefBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream);
	int UrlBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream);
	
	int StblBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream);
	int StsdBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream);

	int Avc1BoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream);
	int AvccBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream);
	int Mp4aBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream);
	int EsdsBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream);

	int SttsBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream);
	int StscBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream);
	int StszBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream);
	int StcoBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream);


	int MvexBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream);
	int MehdBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream);
	int TrexBoxWrite(std::shared_ptr<std::vector<uint8_t>> &data_stream);

private :
	uint32_t	_duration;
	uint32_t	_timescale;
	uint32_t	_track_id;

	uint32_t	_video_width;
	uint32_t	_video_height;
	// avc Configuration
	std::shared_ptr<std::vector<uint8_t>> _avc_sps;
	std::shared_ptr<std::vector<uint8_t>> _avc_pps;
	 
	uint16_t    _audio_channels;
	uint16_t    _audio_sample_size;
	uint16_t    _audio_sample_rate;
	uint32_t    _audio_sample_index;

	std::string _language;
	std::string _handler_type;
	std::string	_compressor_name;
};