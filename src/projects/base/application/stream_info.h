#pragma once

#include "base/common_types.h"
#include "media_track.h"

class StreamInfo
{
public:
	// 입력 소스의 타입
	enum InputSourceType
	{
		kSourceTypeOrigin = 0,
		kSourceTypeTranscoded 
	};

	StreamInfo();
	StreamInfo(uint32_t stream_id);
	StreamInfo(const StreamInfo &T);

	virtual ~StreamInfo();

	void 		SetId(uint32_t id);
	uint32_t 	GetId();

	ov::String	GetName();
	void 		SetName(ov::String name);

	bool 		AddTrack(std::shared_ptr<MediaTrack> track);

	std::shared_ptr<MediaTrack> GetTrackAt(uint32_t index);
	std::shared_ptr<MediaTrack> GetTrack(uint32_t id);
	uint32_t 	GetTrackCount();

	void 		ShowInfo();


private:
	uint32_t 	_id;
	ov::String	_name;
	
	// MediaTrack ID 값을 Key로 활용함
	std::map<int32_t, std::shared_ptr<MediaTrack>> _tracks;

};
