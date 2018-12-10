//==============================================================================
//
//  OvenMediaEngine
//
//  Created by Jaejong Bong
//  Copyright (c) 2018 AirenSoft. All rights reserved.
//
//==============================================================================

#pragma once
#include <map>
#include "packetyzer_define.h"

//====================================================================================================
// Packetyzer
//====================================================================================================
class Packetyzer
{
public:
	Packetyzer(	PacketyzerType 			packetyzer_type,
				std::string 			&segment_prefix,
				PacketyzerStreamType 	stream_type,
				uint32_t 				segment_count,
				uint32_t 				segment_duration,
				PacketyzerMediaInfo 	&media_info);
	virtual ~Packetyzer();

public :
	static uint64_t ConvertTimeScale(uint64_t time, uint32_t from_timescale, uint32_t to_timescale);
	bool SetPlayList(std::string &play_list);
	bool SetSegmentData(	SegmentDataType data_type,
							uint32_t 		sequence_number,
							std::string 	file_name,
							uint64_t 		duration,
							uint64_t 		timestamp_,
							std::shared_ptr<std::vector<uint8_t>> &data,
							bool 			indexing = true);
	bool GetPlayList(std::string &play_list);
	bool GetSegmentData(std::string &file_name, std::shared_ptr<std::vector<uint8_t>> &data);
   
protected :
	PacketyzerType			_packetyzer_type;
	std::string				_segment_prefix;
	PacketyzerStreamType	_stream_type;
	int 					_segment_count;
	int						_segment_save_count;
	uint64_t				_segment_duration; // second
	PacketyzerMediaInfo		_media_info;
	uint32_t				_sequence_number;
	uint32_t				_video_sequence_number;
	uint32_t				_audio_sequence_number;
	bool					_save_file;
	std::string 			_play_list;
	bool 					_video_init;
	bool 					_audio_init;

	std::map<std::string, std::shared_ptr<SegmentData>> _segment_datas;
	std::deque<std::string>								_segment_indexer; 			// (Video+Audio)Segment Name 인덱서(순차적 데이터 삭제를 위해 사용) - TS
	std::deque<std::string>								_video_segment_indexer; 	// Video Segment Name 인덱서(순차적 데이터 삭제를 위해 사용)  - M3S(Video)
	std::deque<std::string>								_audio_segment_indexer; 	// Audio Segment Name 인덱서(순차적 데이터 삭제를 위해 사용)  - M3S(Audio)
	std::mutex 											_segment_datas_mutex;
};